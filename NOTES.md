 Assumptions in the code / Design decisions:

 - All ASCII for now, no real UTF8
 - The cursor is only ever on a valid buffer location.  This can be the hidden CR of the line,
   or the 0 at the end.
 - Even if the loaded buffer doesn't 0 terminate, it is terminated and then removed at save time if necessary.
 - Internally every thing is a '\n'; '\r\n' is converted and converted back if necessary.  Mixed files aren't supported, you'll just get a '\n' file
 - The window converts the entire buffer to a list of visible character lines.  This is used to handle wrapping, and let the user jump around in a wrapped buffer file.
 - Cursor is on the virtual line in the wrapped buffer, and can be off screen; but ScrollTo will find it