// The menu in the Text language, looked up by its English text: the voice (help,
// questions, messages) in lang/help.tsv (src/lang_help.inc), and what is drawn in the
// game font (labels, values, titles, answers) and the legend's words in lang/menu.tsv
// (src/lang_menu.inc). More options' tuning rows stay English.
#pragma once

enum { LANG_COUNT = 6 };  // English, Francais, Deutsch, Espanol, Italiano, Portugues: the Text row's order

// The text in that language, CP850; the English itself when the table has no row for it.
const char *Lang_Tr(const char *english, int lang);
// The voice text's row in lang/help.tsv for this language, or null when it has none.
const char *Lang_Find(const char *english, int lang);
// A label in that language, CP850; nullptr when lang/menu.tsv has no row for it.
const char *Lang_Label(const char *english, int lang);
// The voice table, for the self test: how many texts, and one in a language.
int Lang_Count();
const char *Lang_Text(int i, int lang);
// The label table, likewise.
int Lang_LabelCount();
const char *Lang_LabelText(int i, int lang);
