/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utilities */

char *new_string(char *str) {
  char *new_str = (char *) malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

int init_words(WordCount **wclist) {
  /* Initialize word count.
     Returns 0 if no errors are encountered
     in the body of this function; 1 otherwise.
  */
  //wclist = (WordCount**) malloc(sizeof(WordCount*));
  //*wclist = (WordCount*) malloc(sizeof(WordCount));
  *wclist = NULL;

  if (!wclist) {
    return 1;
  }
  return 0;
}

ssize_t len_words(WordCount *wchead) {
  /* Return -1 if any errors are
     encountered in the body of
     this function.
  */
    int counter = 0;
    if (!wchead){
      return -1;
    }
    while (wchead != NULL) {
      wchead = wchead->next;
      counter++;

    }
    return counter;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */
  WordCount *wc = wchead;
  while (wc != NULL) {
    if (strcmp(wc->word, word) == 0) {
      //printf("find word Debug: %s \n", wc->word);
      return wc;
    } else {
      wc = wc->next;
    }
  }
  return NULL;
}

int add_word(WordCount **wclist, char *word) {
  /* If word is present in word_counts list, increment the count.
     Otherwise insert with count 1.
     Returns 0 if no errors are encountered in the body of this function; 1 otherwise.
  */
 WordCount *wc = find_word(*wclist, word);
 
 size_t length = len_words(*wclist);
 if (wc != NULL) {
   wc->count += 1;
   return 0;
 } else {
   if (length == 0) {
    *wclist = malloc(sizeof(WordCount));
    if (!(wclist)) {
      return 1;
    }
    (*wclist)->next = NULL;
   } else {
    WordCount *new_wordcount = malloc(sizeof(WordCount));
    if (!(new_wordcount)) {  
      return 1;
    }
    (*new_wordcount).next = *wclist;
    *wclist = new_wordcount;
    //(*new_wordcount).word = *wclist;
   }
   (*wclist)->word = word;
   (*wclist)->count = 1;
   return 0;
 }
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}