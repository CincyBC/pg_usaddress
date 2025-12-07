#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include "crfsuite_wrapper.h"

/*
 * extract_features
 * input: raw string
 * output: array of CrfSuiteItem
 * sets *num_items to length of array
 * returns array pointer (caller must free), or NULL on error
 */
CrfSuiteItem *extract_features(const char *input, int *num_items);

/*
 * free_features
 * frees the array returned by extract_features
 */
void free_features(CrfSuiteItem *items, int num_items);

/*
 * Standardizes a token (like to lower case, etc if needed for returns)
 * but primarily we just need tokens for the output.
 * We might need a function to get the raw tokens back if we construct the
 * result? Actually, the current design separates feature extraction (for model)
 * from tokenization (for result). We probably want `extract_features` to also
 * return the list of raw tokens so we can pair them with labels later.
 */

typedef struct {
  char *token;
  CrfSuiteItem features;
} TokenFeatures;

TokenFeatures *tokenize_and_extract_features(const char *input, int *num_items);

void free_token_features(TokenFeatures *items, int num_items);

#endif
