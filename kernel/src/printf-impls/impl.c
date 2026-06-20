//shitty temporaryfix
#include "Flanterm/src/flanterm.h"
#include "printf-impls/conf.h"
char last_char; 
void _putchar(char character) {
    if(character =='\n' && last_char !='\r') {
        flanterm_write(ft_ctx,"\r\n",2);
    } else {
        flanterm_write(ft_ctx,(char*)&character,1);
    }
    last_char = character;
}