#ifndef CRFSUITE_WRAPPER_H
#define CRFSUITE_WRAPPER_H

#include <stddef.h>

/* Opaque struct to hide crfsuite details from consumers */
typedef struct CrfSuiteModel CrfSuiteModel;

/*
 * Represents a single item in a sequence to be tagged.
 * Contains an array of features (strings).
 */
typedef struct {
  char **features;
  int num_features;
} CrfSuiteItem;

/*
 * Creates a model instance from a file.
 * Returns NULL on failure.
 */
CrfSuiteModel *crfsuite_model_create(const char *filename);

/*
 * Tags a sequence of items.
 * returns an array of label strings, or NULL on error.
 * The caller is responsible for freeing the returned array (but not the strings
 * inside it, usually labels are static or managed by the model, but actually
 * crfsuite returns copies or internal references. We should standardize on
 * returning newly allocated strings or references. Given PG constraints, let's
 * return a list of strings that the caller must free, OR manage memory
 * carefully.
 *
 * Ideally, we return an array of strings.
 */
int crfsuite_model_tag(CrfSuiteModel *model, CrfSuiteItem *items, int num_items,
                       char ***labels_out);

/*
 * Frees the model.
 */
void crfsuite_model_destroy(CrfSuiteModel *model);

#endif
