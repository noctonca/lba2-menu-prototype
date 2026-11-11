// The voice in the Text language: help, questions and messages, looked up by their
// English text in lang/help.tsv (src/lang_table.inc). Row labels and values stay English.
#pragma once

enum { LANG_COUNT = 6 };  // English, Francais, Deutsch, Espanol, Italiano, Portugues: the Text row's order

// The text in that language, CP850; the English itself when the table has no row for it.
const char *Lang_Tr(const char *english, int lang);
// The table, for the self test: how many texts, and one in a language.
int Lang_Count();
const char *Lang_Text(int i, int lang);
