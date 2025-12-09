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
  CrfSuiteModel *wrapper = malloc(sizeof(CrfSuiteModel));
  if (!wrapper)
    return NULL;

  wrapper->model = NULL;
  wrapper->tagger = NULL;
  wrapper->attrs = NULL;
  wrapper->labels = NULL;

  int ret =
      crfsuite_create_instance_from_file(filename, (void **)&wrapper->model);
  if (ret != 0 || wrapper->model == NULL) {
    free(wrapper);
    return NULL;
  }

  ret = wrapper->model->get_tagger(wrapper->model, &wrapper->tagger);
  if (ret != 0 || wrapper->tagger == NULL) {
    wrapper->model->release(wrapper->model);
    free(wrapper);
    return NULL;
  }

  wrapper->model->get_attrs(wrapper->model, &wrapper->attrs);
  wrapper->model->get_labels(wrapper->model, &wrapper->labels);

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
  if (!wrapper || !wrapper->tagger || !items || num_items <= 0) {
    return -1;
  }

  crfsuite_tagger_t *tagger = wrapper->tagger;
  crfsuite_dictionary_t *attrs = wrapper->attrs;
  crfsuite_dictionary_t *labels = wrapper->labels;

  crfsuite_instance_t inst;

  crfsuite_instance_init(&inst);
  inst.num_items = num_items;
  inst.items = calloc(num_items, sizeof(crfsuite_item_t));

  if (!inst.items) {
    return -1;
  }

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
      int aid = attrs->to_id(attrs, items[i].features[j]);
      if (aid < 0) {
        /* Skip unknown attributes instead of using -1 */
        continue;
      }
      crfsuite_attribute_set(
          &inst.items[i].contents[inst.items[i].num_contents], aid, 1.0);
      inst.items[i].num_contents++;
    }
  }

  int ret = tagger->set(tagger, &inst);

  if (ret != 0) {
    crfsuite_instance_finish(&inst);
    return -1;
  }

  *labels_out = malloc(num_items * sizeof(char *));
  if (!*labels_out) {
    crfsuite_instance_finish(&inst);
    return -1;
  }

  int *path = calloc(num_items, sizeof(int));
  if (!path) {
    free(*labels_out);
    crfsuite_instance_finish(&inst);
    return -1;
  }
  floatval_t score = 0.0;
  ret = tagger->viterbi(tagger, path, &score);

  if (ret != 0) {
    free(path);
    free(*labels_out);
    crfsuite_instance_finish(&inst);
    return -1;
  }

  for (int i = 0; i < num_items; i++) {
    const char *label_str = NULL;
    labels->to_string(labels, path[i], &label_str);

    if (label_str) {
      (*labels_out)[i] = strdup(label_str);
    } else {
      (*labels_out)[i] = strdup("");
    }
  }

  free(path);
  crfsuite_instance_finish(&inst);
  return 0;
}
