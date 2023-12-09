#include <stdio.h>

int main() {
  FILE *fp;
  fp = fopen("mnt/mnt/data1.txt", "w");
  if (fp < 0)
    return -1;

  else {
    fclose(fp);
    return 0;
  }
}