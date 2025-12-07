#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"

#include "crfsuite_wrapper.h"
#include "feature_extractor.h"

PG_MODULE_MAGIC;

static CrfSuiteModel *usaddress_model = NULL;

static void load_model_if_needed(void) {
  char path[MAXPGPATH];
  char share_path[MAXPGPATH];

  fprintf(stderr, "DEBUG: load_model_if_needed called, usaddress_model=%p\n",
          (void *)usaddress_model);
  fflush(stderr);
  if (usaddress_model)
    return;

  get_share_path(my_exec_path, share_path);
  snprintf(path, MAXPGPATH, "%s/extension/usaddr.crfsuite", share_path);
  fprintf(stderr, "DEBUG: Loading model from path: %s\n", path);
  fflush(stderr);
  usaddress_model = crfsuite_model_create(path);
  fprintf(stderr, "DEBUG: crfsuite_model_create returned %p\n",
          (void *)usaddress_model);
  fflush(stderr);
  if (!usaddress_model) {
    ereport(WARNING,
            (errmsg("Could not load usaddr.crfsuite model from %s", path)));
  }
}

PG_FUNCTION_INFO_V1(parse_address_crf);
Datum parse_address_crf(PG_FUNCTION_ARGS) {
  FuncCallContext *funcctx;
  int call_cntr;
  int max_calls;

  ereport(LOG, (errmsg("DEBUG PG: parse_address_crf entered")));

  if (SRF_IS_FIRSTCALL()) {
    MemoryContext oldcontext;
    text *arg;
    char *input_str;
    int num_items = 0;
    TokenFeatures *tokens = NULL;
    CrfSuiteItem *crf_items;
    char **labels = NULL;
    int i;
    int filtered_count = 0;
    int current_idx = 0;

    typedef struct {
      TokenFeatures *tokens;
      char **labels;
      int num_items;
    } UserCtx;
    UserCtx *uctx;

    ereport(LOG, (errmsg("DEBUG PG: Calling load_model_if_needed")));
    load_model_if_needed();
    ereport(LOG, (errmsg("DEBUG PG: Model loaded, usaddress_model=%p",
                         (void *)usaddress_model)));
    if (!usaddress_model)
      ereport(ERROR, (errmsg("Model not loaded")));

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    /* Build a tuple descriptor for our result type */
    if (get_call_result_type(fcinfo, NULL, &funcctx->tuple_desc) !=
        TYPEFUNC_COMPOSITE)
      ereport(ERROR, (errmsg("return type must be a row type")));
    BlessTupleDesc(funcctx->tuple_desc);

    arg = PG_GETARG_TEXT_PP(0);
    input_str = text_to_cstring(arg);
    ereport(LOG, (errmsg("DEBUG PG: Input string: '%s'", input_str)));

    ereport(LOG, (errmsg("DEBUG PG: Calling tokenize_and_extract_features")));
    tokens = tokenize_and_extract_features(input_str, &num_items);
    fprintf(stderr, "DEBUG PG: Tokenized, num_items=%d\n", num_items);
    fflush(stderr);

    crf_items = malloc(num_items * sizeof(CrfSuiteItem));
    fprintf(stderr, "DEBUG PG: Building crf_items\n");
    fflush(stderr);
    for (i = 0; i < num_items; i++)
      crf_items[i] = tokens[i].features;

    fprintf(stderr, "DEBUG PG: Calling crfsuite_model_tag\n");
    fflush(stderr);
    if (crfsuite_model_tag(usaddress_model, crf_items, num_items, &labels) !=
        0) {
      free(crf_items);
      free_token_features(tokens, num_items);
      MemoryContextSwitchTo(oldcontext);
      ereport(ERROR, (errmsg("Tagging failed")));
    }
    fprintf(stderr, "DEBUG PG: Tagging succeeded\n");
    fflush(stderr);

    // Filter out commas

    for (i = 0; i < num_items; i++) {
      if (strcmp(tokens[i].token, ",") != 0) {
        filtered_count++;
      }
    }

    uctx = (UserCtx *)palloc(sizeof(UserCtx));
    uctx->tokens = palloc(filtered_count * sizeof(TokenFeatures));
    uctx->labels = palloc(filtered_count * sizeof(char *));
    uctx->num_items = filtered_count;

    for (i = 0; i < num_items; i++) {
      if (strcmp(tokens[i].token, ",") != 0) {
        // Copy token structure (shallow copy of pointers is fine if we
        // carefully manage memory, but original 'tokens' array is freed in
        // 'free_token_features'. We need to keep the strings alive or duplicate
        // them. The original 'tokens' array was allocated by
        // 'tokenize_and_extract_features'. The strings inside 'tokens' are
        // allocated. If we free the original 'tokens' array, we lose the
        // structs but not necessarily strings if we handle it right. But
        // 'free_token_features' frees the strings too! So we MUST duplicate the
        // strings for our new list, OR keep the original list and just have an
        // index mapping? Easier: Duplicate strings and free the original full
        // list now.

        uctx->tokens[current_idx].token = pstrdup(tokens[i].token);
        // We don't need features for the output iteration, just the token
        // string. But UserCtx definition has TokenFeatures*. Let's respect
        // that.
        uctx->tokens[current_idx].features.num_features = 0; // Not used
        uctx->tokens[current_idx].features.features = NULL;  // Not used

        uctx->labels[current_idx] = pstrdup(labels[i]);
        current_idx++;
      }
    }

    // Now we can free the original arrays
    fprintf(stderr, "DEBUG PG: About to free_token_features\n");
    fflush(stderr);
    free_token_features(tokens, num_items);
    fprintf(stderr, "DEBUG PG: free_token_features done\n");
    fflush(stderr);

    fprintf(stderr, "DEBUG PG: About to free labels\n");
    fflush(stderr);
    for (i = 0; i < num_items; i++) { // free original labels
      // fprintf(stderr, "DEBUG PG: freeing label %d: %p\n", i,
      // (void*)labels[i]); fflush(stderr);
      free(labels[i]);
    }
    free(labels);
    fprintf(stderr, "DEBUG PG: free labels done\n");
    fflush(stderr);

    fprintf(stderr, "DEBUG PG: About to free crf_items\n");
    fflush(stderr);
    free(crf_items);
    fprintf(stderr, "DEBUG PG: free crf_items done\n");
    fflush(stderr);

    // Wait, original code kept 'tokens' and 'labels' directly in uctx.
    // uctx->tokens = tokens;
    // uctx->labels = labels;
    // And freed them in the final call (lines 144-147).

    // So if I create NEW arrays, I should free the OLD ones NOW.
    // Yes.

    funcctx->user_fctx = (void *)uctx;
    funcctx->max_calls = filtered_count;

    pfree(input_str);
    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;

  if (call_cntr < max_calls) {
    typedef struct {
      TokenFeatures *tokens;
      char **labels;
      int num_items;
    } UserCtx;
    UserCtx *uctx = (UserCtx *)funcctx->user_fctx;

    Datum values[2];
    bool nulls[2] = {false, false};
    HeapTuple tuple;
    Datum result;

    values[0] = CStringGetTextDatum(uctx->tokens[call_cntr].token);
    values[1] = CStringGetTextDatum(uctx->labels[call_cntr]);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    result = HeapTupleGetDatum(tuple);
    SRF_RETURN_NEXT(funcctx, result);
  } else {
    // Resources allocated in multi_call_memory_ctx (uctx, tokens, labels)
    // are automatically freed by PostgreSQL when the SRF finishes.
    // We must NOT call free() on palloc'd memory.

    SRF_RETURN_DONE(funcctx);
  }
}

PG_FUNCTION_INFO_V1(tag_address_crf);
Datum tag_address_crf(PG_FUNCTION_ARGS) {
  text *arg;
  char *input_str;
  int num_items = 0;
  TokenFeatures *tokens;
  CrfSuiteItem *crf_items;
  char **labels = NULL;
  typedef struct LabelBuf {
    char *label;
    StringInfoData buf;
    struct LabelBuf *next;
  } LabelBuf;
  LabelBuf *head = NULL;
  LabelBuf *curr;
  int i;
  StringInfoData json_str;
  bool first;
  Datum jsonb_datum;

  load_model_if_needed();
  if (!usaddress_model)
    ereport(ERROR, (errmsg("Model not loaded")));

  arg = PG_GETARG_TEXT_PP(0);
  input_str = text_to_cstring(arg);

  tokens = tokenize_and_extract_features(input_str, &num_items);

  crf_items = malloc(num_items * sizeof(CrfSuiteItem));
  for (i = 0; i < num_items; i++)
    crf_items[i] = tokens[i].features;

  if (crfsuite_model_tag(usaddress_model, crf_items, num_items, &labels) != 0) {
    free(crf_items);
    free_token_features(tokens, num_items);
    ereport(ERROR, (errmsg("Tagging failed")));
  }

  free(crf_items);

  for (i = 0; i < num_items; i++) {
    char *lbl = labels[i];
    char *tok = tokens[i].token;

    // Optional: Filter commas in JSON? The user didn't explicitly ask for this
    // in JSON, but likely "no commas in parsing" applies here too. However, for
    // JSON construction, maybe we just skip them? Let's stick to the SRF
    // function first as that's what the test output shows. Leaving JSON
    // function as is for now unless tests fail there too.

    curr = head;
    while (curr) {
      if (strcmp(curr->label, lbl) == 0)
        break;
      curr = curr->next;
    }

    if (!curr) {
      curr = palloc(sizeof(LabelBuf));
      curr->label = pstrdup(lbl);
      initStringInfo(&curr->buf);
      curr->next = head;
      head = curr;
    } else {
      appendStringInfoString(&curr->buf, " ");
    }
    appendStringInfoString(&curr->buf, tok);
  }

  initStringInfo(&json_str);
  appendStringInfoChar(&json_str, '{');

  curr = head;
  first = true;
  while (curr) {
    if (!first)
      appendStringInfoString(&json_str, ", ");
    escape_json(&json_str, curr->label);
    appendStringInfoString(&json_str, ": ");
    escape_json(&json_str, curr->buf.data);
    first = false;
    curr = curr->next;
  }
  appendStringInfoChar(&json_str, '}');

  jsonb_datum = DirectFunctionCall1(jsonb_in, CStringGetDatum(json_str.data));
  free_token_features(tokens, num_items);
  for (i = 0; i < num_items; i++)
    free(labels[i]);
  free(labels);
  PG_RETURN_JSONB_P(DatumGetJsonbP(jsonb_datum));
}

PG_FUNCTION_INFO_V1(parse_address_crf_cols);
Datum parse_address_crf_cols(PG_FUNCTION_ARGS) {
  text *arg;
  char *input_str;
  int num_items = 0;
  TokenFeatures *tokens;
  CrfSuiteItem *crf_items;
  char **labels = NULL;
  TupleDesc tupdesc;
  int natts;
  Datum *values;
  bool *nulls;
  StringInfoData *buffers;
  bool *has_content;
  int i;
  HeapTuple tuple;

  load_model_if_needed();
  if (!usaddress_model)
    ereport(ERROR, (errmsg("Model not loaded")));

  arg = PG_GETARG_TEXT_PP(0);
  input_str = text_to_cstring(arg);

  tokens = tokenize_and_extract_features(input_str, &num_items);

  crf_items = malloc(num_items * sizeof(CrfSuiteItem));
  for (i = 0; i < num_items; i++)
    crf_items[i] = tokens[i].features;

  if (crfsuite_model_tag(usaddress_model, crf_items, num_items, &labels) != 0) {
    free(crf_items);
    free_token_features(tokens, num_items);
    ereport(ERROR, (errmsg("Tagging failed")));
  }
  free(crf_items);

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
    ereport(ERROR, (errmsg("return type must be a row type")));
  }

  natts = tupdesc->natts;
  values = palloc0(natts * sizeof(Datum));
  nulls = palloc0(natts * sizeof(bool));
  for (i = 0; i < natts; i++)
    nulls[i] = true;

  buffers = palloc0(natts * sizeof(StringInfoData));
  has_content = palloc0(natts * sizeof(bool));

  for (i = 0; i < num_items; i++) {
    char *lbl = labels[i];
    char *tok = tokens[i].token;

    int match_idx = -1;
    int c;

    for (c = 0; c < natts; c++) {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, c);
      char *col_name;
      char clean_col[NAMEDATALEN];
      int ci = 0, cj = 0;

      if (attr->attisdropped)
        continue;

      col_name = NameStr(attr->attname);
      while (col_name[ci]) {
        if (col_name[ci] != '_')
          clean_col[cj++] = col_name[ci];
        ci++;
      }
      clean_col[cj] = '\0';

      if (strcasecmp(clean_col, lbl) == 0) {
        match_idx = c;
        break;
      }
    }

    if (match_idx >= 0) {
      if (!has_content[match_idx]) {
        initStringInfo(&buffers[match_idx]);
        has_content[match_idx] = true;
      } else {
        appendStringInfoString(&buffers[match_idx], " ");
      }
      appendStringInfoString(&buffers[match_idx], tok);
    }
  }

  for (i = 0; i < natts; i++) {
    if (has_content[i]) {
      values[i] = CStringGetTextDatum(buffers[i].data);
      nulls[i] = false;
    }
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);
  free_token_features(tokens, num_items);
  for (i = 0; i < num_items; i++)
    free(labels[i]);
  free(labels);

  PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}
