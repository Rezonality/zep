 Assumptions in the code / Design decisions:

 - All ASCII for now, now UTF8
 - The cursor is only ever on a valid buffer location.  This can be the hidden CR of the line, or the 0 at the end.
 - Even if the loaded buffer doesn't 0 terminate, it is terminated and then removed at save time if necessary.
 - Internally every thing is a '\n'; '\r\n' is converted and converted back if necessary.  Mixed files aren't supported, you'll just get a '\n' file