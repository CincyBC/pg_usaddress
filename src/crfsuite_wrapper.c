#include "crfsuite_wrapper.h"
#include <crfsuite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CrfSuiteModel {
  crfsuite_model_t *model;
  crfsuite_tagger_t *tagger;
  crfsuite_dictionary_t *attrs;
  crfsuite_dictionary_t *labels;
};

CrfSuiteModel *crfsuite_model_create(const char *filename) {
  fprintf(stderr, "DEBUG CRF: crfsuite_model_create(%s)\n", filename);
  fflush(stderr);
  CrfSuiteModel *wrapper = malloc(sizeof(CrfSuiteModel));
  if (!wrapper)
    return NULL;

  wrapper->model = NULL;
  wrapper->tagger = NULL;
  wrapper->attrs = NULL;
  wrapper->labels = NULL;

  fprintf(stderr, "DEBUG CRF: Calling crfsuite_create_instance_from_file\n");
  fflush(stderr);
  int ret =
      crfsuite_create_instance_from_file(filename, (void **)&wrapper->model);
  fprintf(
      stderr,
      "DEBUG CRF: crfsuite_create_instance_from_file returned %d, model=%p\n",
      ret, (void *)wrapper->model);
  fflush(stderr);
  if (ret != 0 || wrapper->model == NULL) {
    free(wrapper);
    return NULL;
  }

  fprintf(stderr, "DEBUG CRF: Calling get_tagger\n");
  fflush(stderr);
  ret = wrapper->model->get_tagger(wrapper->model, &wrapper->tagger);
  fprintf(stderr, "DEBUG CRF: get_tagger returned %d, tagger=%p\n", ret,
          (void *)wrapper->tagger);
  fflush(stderr);
  if (ret != 0 || wrapper->tagger == NULL) {
    wrapper->model->release(wrapper->model);
    free(wrapper);
    return NULL;
  }

  fprintf(stderr, "DEBUG CRF: Getting attrs and labels\n");
  fflush(stderr);
  wrapper->model->get_attrs(wrapper->model, &wrapper->attrs);
  wrapper->model->get_labels(wrapper->model, &wrapper->labels);
  fprintf(stderr, "DEBUG CRF: attrs=%p, labels=%p\n", (void *)wrapper->attrs,
          (void *)wrapper->labels);
  fflush(stderr);

  return wrapper;
}

void crfsuite_model_destroy(CrfSuiteModel *wrapper) {
  if (wrapper) {
    if (wrapper->labels)
      wrapper->labels->release(wrapper->labels);
    if (wrapper->attrs)
      wrapper->attrs->release(wrapper->attrs);
    if (wrapper->tagger)
      wrapper->tagger->release(wrapper->tagger);
    if (wrapper->model)
      wrapper->model->release(wrapper->model);
    free(wrapper);
  }
}

int crfsuite_model_tag(CrfSuiteModel *wrapper, CrfSuiteItem *items,
                       int num_items, char ***labels_out) {
  fprintf(stderr, "DEBUG: crfsuite_model_tag entered, num_items=%d\n",
          num_items);
  fflush(stderr);

  if (!wrapper || !wrapper->tagger || !items || num_items <= 0) {
    fprintf(stderr, "DEBUG: Early return - invalid params\n");
    return -1;
  }

  fprintf(stderr, "DEBUG: Getting tagger/attrs/labels\n");
  fflush(stderr);

  crfsuite_tagger_t *tagger = wrapper->tagger;
  crfsuite_dictionary_t *attrs = wrapper->attrs;
  crfsuite_dictionary_t *labels = wrapper->labels;

  fprintf(stderr, "DEBUG: tagger=%p, attrs=%p, labels=%p\n", (void *)tagger,
          (void *)attrs, (void *)labels);
  fflush(stderr);

  crfsuite_instance_t inst;

  fprintf(stderr, "DEBUG: Calling crfsuite_instance_init\n");
  fflush(stderr);
  crfsuite_instance_init(&inst);
  inst.num_items = num_items;
  inst.items = calloc(num_items, sizeof(crfsuite_item_t));

  if (!inst.items) {
    fprintf(stderr, "DEBUG: Failed to allocate inst.items\n");
    return -1;
  }

  fprintf(stderr, "DEBUG: Building instance items\n");
  fflush(stderr);

  for (int i = 0; i < num_items; i++) {
    crfsuite_item_init(&inst.items[i]);
    inst.items[i].num_contents = 0;
    inst.items[i].contents =
        calloc(items[i].num_features, sizeof(crfsuite_attribute_t));

    if (!inst.items[i].contents) {
      crfsuite_instance_finish(&inst);
      return -1;
    }

    for (int j = 0; j < items[i].num_features; j++) {
      fprintf(stderr, "DEBUG: Item %d, feature %d: %s\n", i, j,
              items[i].features[j] ? items[i].features[j] : "(null)");
      fflush(stderr);
      int aid = attrs->to_id(attrs, items[i].features[j]);
      fprintf(stderr, "DEBUG: Attribute ID for '%s' = %d\n",
              items[i].features[j], aid);
      fflush(stderr);
      if (aid < 0) {
        /* Skip unknown attributes instead of using -1 */
        fprintf(stderr, "DEBUG: Skipping unknown attribute\n");
        fflush(stderr);
        continue;
      }
      crfsuite_attribute_set(
          &inst.items[i].contents[inst.items[i].num_contents], aid, 1.0);
      inst.items[i].num_contents++;
    }
    fprintf(stderr, "DEBUG: Item %d has %d known attributes\n", i,
            inst.items[i].num_contents);
    fflush(stderr);
  }

  fprintf(stderr, "DEBUG: Calling tagger->set\n");
  fflush(stderr);

  int ret = tagger->set(tagger, &inst);
  fprintf(stderr, "DEBUG: tagger->set returned %d\n", ret);
  fflush(stderr);

  if (ret != 0) {
    crfsuite_instance_finish(&inst);
    return -1;
  }

  *labels_out = malloc(num_items * sizeof(char *));
  if (!*labels_out) {
    crfsuite_instance_finish(&inst);
    return -1;
  }

  fprintf(stderr, "DEBUG: Calling tagger->viterbi\n");
  fflush(stderr);

  int *path = calloc(num_items, sizeof(int));
  floatval_t score = 0.0;
  ret = tagger->viterbi(tagger, path, &score);

  fprintf(stderr, "DEBUG: tagger->viterbi returned %d, score=%f\n", ret, score);
  fflush(stderr);

  if (ret != 0) {
    free(path);
    free(*labels_out);
    crfsuite_instance_finish(&inst);
    return -1;
  }

  fprintf(stderr, "DEBUG: Converting path to labels\n");
  fflush(stderr);

  for (int i = 0; i < num_items; i++) {
    const char *label_str = NULL;
    fprintf(stderr, "DEBUG: Getting label for path[%d]=%d\n", i, path[i]);
    fflush(stderr);
    labels->to_string(labels, path[i], &label_str);

    if (label_str) {
      (*labels_out)[i] = strdup(label_str);
    } else {
      (*labels_out)[i] = strdup("");
    }
  }

  fprintf(stderr, "DEBUG: Cleaning up and returning success\n");
  fflush(stderr);

  free(path);
  crfsuite_instance_finish(&inst);
  return 0;
}
