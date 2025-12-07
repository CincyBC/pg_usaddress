// Stub for crf1de_create_instance since we excluded training interface
// This function is declared in crfsuite.c but not used for inference-only
// builds
int crf1de_create_instance(const char *iid, void **ptr) {
  // Return 1 to indicate failure - this won't be called for tagger creation
  return 1;
}
