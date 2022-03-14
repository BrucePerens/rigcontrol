struct user_data {
  char * data;
  size_t size;
  size_t index;
  void   (*coroutine)(const char *, size_t);
};

