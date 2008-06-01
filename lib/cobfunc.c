/* cobfunc.c
**
**   Defines cobol function library.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <endian.h>
#include <math.h>
#include <stdlib.h>
#include "cobfunc.h"

#ifndef min
#define min(a,b)	((a) < (b) ? (a) : (b))
#endif

char _strbuf[201];		/* For ---toString routines. This is the
				   buffer */

static int _IsInSet(char c, char *set)
{
    int i;

    for (i = 0; i < strlen(set); ++i)
	if (set[i] == c)
	    return (1);		/* YES */
    return (0);			/* NO */
}

int _Alphabetic(char *str)
{
    int i;

    for (i = 0; i < strlen(str); i++)
	if (!(isalpha(str[i]) || str[i] == ' '))
	    return (0);
    return (1);
}

/*
** what = 1  : lowercase test
** what = 2  : uppercase test 
*/
int _AlphabeticCase(char *str, int what)
{
int i;
    if (_Alphabetic(str)) {
	for (i = 0; i < strlen(str); i++) {
	    if (what == 2 && islower(str[i]))
		return (0);
	    else if (what == 1 && isupper(str[i]))
		return (0);
	}
    }
    else
	return (0);
    return (1);
}

void _RuntimeError(char *message)
{
    printf("\nRuntime error: %s!\n\n", message);
    exit(1);
}

void _ReadStringVar(FILE * stream, char *var, char *pic)
{
int si, pi, sl, pl;
char c;

    pl = strlen(pic);
    sl = 0;
    /* Find length of the string by excluding editing characters from pic */
    for (pi = 0; pi < pl; ++pi)
	if (!_IsInSet(pic[pi], ".,/bB0"))
	    ++sl;

    /* All string variables are spaces by default */
    memset(var, ' ', sl);

    /* Read characters until a count of chars is reached */
    si = 0;

    /* Iterate over picture skipping all edited positions */
    for (pi = 0; pi < pl; ++pi) {
	c = fgetc(stream);
	if (c == '\n' || c == '\x0' || feof(stream))
	    break;
	if (!_IsInSet(pic[pi], ".,/bB0")) {
	    var[si] = c;
	    ++si;
	}
    }
}

void _ReadIntegerVar(FILE * stream, long int *var, char *pic)
{
char buffer[20];

    _ReadStringVar(stream, buffer, pic);
    *var = atol(buffer);
}

void _ReadFloatVar(FILE * stream, double *var, char *pic)
{
char buffer[20];

    _ReadStringVar(stream, buffer, pic);
    *var = atof(buffer);
}

void _WriteStringVar(FILE * stream, char *var, char *pic)
{
int i, j;

    for (i = 0, j = 0; i < strlen(pic); ++i) {
	switch (pic[i]) {
	case 'x':
	    fprintf(stream, "%c", var[j++]);
	    break;
	case 'b':
	    fprintf(stream, " ");
	    break;
	case '/':
	    fprintf(stream, "/");
	    break;
	case '0':
	    fprintf(stream, "0");
	    break;
	default:
	    fprintf(stream, "%c", pic[i]);
	}
    }
}

void _WriteIntegerVar(FILE * stream, long int var, char *pic)
{
    fprintf(stream, "%s", _IntegerToString(var, pic));
}

void _WriteFloatVar(FILE * stream, double var, char *pic)
{
    fprintf(stream, "%s", _FloatToString(var, pic));
}

/* Result in _strbuf */
char * _IntegerToString(long int var, char *pic)
{
char barenum[20];
int i, j = 0, nl, pl, fl = 0;
int sp = 0;

    sprintf(barenum, "%lu", var);
    nl = strlen(barenum);
    pl = strlen(pic);

    /* Calculate number of filler zeroes on the left */
    for (i = 0; i < pl; ++i)
	if (pic[i] == '9' || pic[i] == 'z' || pic[i] == '*')
	    ++fl;

    /* If the number does not fit, cut leftmost digits */
    if (fl < nl)
	j = nl - fl;

    for (i = 0; i < pl; ++i) {
	if (pic[i] == '9') {
	    if (fl <= nl)
		_strbuf[sp++] = barenum[j++];
	    else
		_strbuf[sp++] = ' ';
	    --fl;
	}
	else if (pic[i] == '.')
	    _strbuf[sp++] = '.';
	else if (pic[i] == ',')
	    _strbuf[sp++] = ',';
	else if (pic[i] == '/')
	    _strbuf[sp++] = '/';
	else if (pic[i] == 'b')
	    _strbuf[sp++] = ' ';
	else if (pic[i] == '0')
	    _strbuf[sp++] = '0';
	else if (pic[i] == '+' || pic[i] == 's' || pic[i] == '-') {
	    if (var > 0 && pic[i] == '-')
		_strbuf[sp++] = '+';
	    else if (var < 0)
		_strbuf[sp++] = '-';
	    else
		_strbuf[sp++] = ' ';
	}
	else if (pic[i] == 'z' || pic[i] == '*')
	    _strbuf[sp++] = '0';
	else if (pic[i] == '$')
	    _strbuf[sp++] = '$';
	else
	    _strbuf[sp++] = (fl - 1 <= nl) ? pic[i] : ' ';
    }
    _strbuf[sp] = '\x0';

    return (_strbuf);
}

/* Result in _strbuf */
char * _FloatToString(double var, char *pic)
{
char format_s[10];
char barenum[20];
int fbp = 0, fap = 0, pl;
int i, j = 0;
int FoundPoint = 0;
int sp = 0;

    pl = strlen(pic);

    /* Calculate number of filler zeroes on the left */
    for (i = 0; i < pl; ++i) {
	if (pic[i] == '9' || pic[i] == '0' || pic[i] == 'z' || pic[i] == '*') {
	    if (FoundPoint)
		++fap;
	    else
		++fbp;
	}
	else if (pic[i] == 'v')
	    FoundPoint = 1;
    }

    sprintf(format_s, "%%%d.%df", fbp, fap);
    sprintf(barenum, format_s, var);

    /* 
       ** sprintf will print correct number of digits after point, ** but
       only as many as needed before the point. ** We thus need to reset
       the index variable j to the real starting point. */
    j = strlen(barenum) - (fbp + 1 + fap);

    for (i = 0; i < pl; ++i) {
	if (pic[i] == '9') {
	    if (j >= 0)
		_strbuf[sp++] = barenum[j];
	    else
		_strbuf[sp++] = ' ';
	    ++j;
	}
	else if (pic[i] == '.')
	    _strbuf[sp++] = '.';
	else if (pic[i] == ',')
	    _strbuf[sp++] = ',';
	else if (pic[i] == '/')
	    _strbuf[sp++] = '/';
	else if (pic[i] == 'b')
	    _strbuf[sp++] = ' ';
	else if (pic[i] == '0')
	    _strbuf[sp++] = '0';
	else if (pic[i] == '+' || pic[i] == 's' || pic[i] == '-') {
	    if (var > 0 && pic[i] == '-')
		_strbuf[sp++] = '+';
	    else if (var < 0)
		_strbuf[sp++] = '-';
	    else
		_strbuf[sp++] = ' ';
	}
	else if (pic[i] == 'z' || pic[i] == '*') {
	    _strbuf[sp++] = '0';
	    ++j;
	}
	else if (pic[i] == 'v') {
	    _strbuf[sp++] = '.';
	    ++j;
	}
	else if (pic[i] == '$')
	    _strbuf[sp++] = '$';
	else
	    _strbuf[sp++] = pic[i];
    }
    _strbuf[sp] = '\x0';

    return (_strbuf);
}

void _AssignVarString(char *var1, char *var2, int p1, int p2)
{
    if (p2 == 0)
	p2 = strlen(var2);

    memset(var1, ' ', p1);
    var1[p1] = '\x0';
    strncpy(var1, var2, min(p1, p2));

    if (p1 > p2)
	var1[p2] = ' ';
}

double _RoundResult(double num, char *pic)
{
    long int tnum;

    tnum = (long int) num;
    if ((num - tnum) < 0.5)
	return (tnum);
    else
	return (tnum + 1);
}

static int _FieldsInSig(char *sig)
{
    int i, IsToken = 0;
    int nTokens = 0;

    for (i = 0; i < strlen(sig); ++i) {
	if (sig[i] == ' ')
	    IsToken = 0;
	else {
	    if (!IsToken)
		++nTokens;
	    IsToken = 1;
	}
    }
    /* Each field is defined by NAME TYPE SIZE DECIMAL */
    return (nTokens / 4);
}

/*
** The syntax is same as strcpy, only that the source pointer
** is moved to the first char after read token. That's what
** char** is for.
*/
static void _TokenCopy(char *dest, char **src)
{
    char c;
    char *sptr, *dptr;

    sptr = *src;		/* These are added just for readability, */
    dptr = dest;		/* like defines                  */

    while ((c = *sptr) != '\x0' && c == ' ')
	++sptr;

    while ((c = *sptr) != '\x0' && c != ' ') {
	*dptr = *sptr;
	++sptr;
	++dptr;
    }
    *dptr = '\x0';		/* Null terminate the string */
    *src = sptr;		/* Update the passed source pointer */
}

static DBF_Field *_SigToFields(char *sig, int *lpnFields)
{
    DBF_Field *Fields;
    char buffer[50];
    char *cursigpos;
    int i, nFields;

    nFields = _FieldsInSig(sig);
    *lpnFields = nFields;

    Fields = (DBF_Field *) calloc(1, nFields * sizeof(DBF_Field));

    cursigpos = sig;
    for (i = 0; i < nFields; ++i) {
	/** First token is the name of the field */
	_TokenCopy(buffer, &cursigpos);
	/* Assuming the last character to be set to 0 by calloc */
	strncpy(Fields[i].Name, buffer, DBF_FIELD_NAME_LENGTH - 1);

	/** Second token is the character denoting type of field */
	_TokenCopy(buffer, &cursigpos);
	Fields[i].Type = buffer[0];

	/** Third token is the size of the field */
	_TokenCopy(buffer, &cursigpos);
	Fields[i].FieldLength = atoi(buffer);

	/** Fourth token is the number of decimal places, if any */
	_TokenCopy(buffer, &cursigpos);
	Fields[i].DecimalPlaces = atoi(buffer);
    }

    /* Fields is freed in DBF_Close */
    return (Fields);
}

void _OpenSequentialFile(FILE ** fp, char *filename, char *mode)
{
char ErrorBuffer[80];

    if ((*fp = fopen(filename, mode)) == NULL) {
	sprintf(ErrorBuffer, "could not open file '%s'", filename);
	_RuntimeError(ErrorBuffer);
    }
}

void _OpenRelativeFile(DBF_FILE ** fp, char *filename, char *sig, char *mode)
{
char ErrorBuffer[80];
DBF_Field *Fields;
int nFields;

    /* If opening for OUTPUT, remove the file first */
    if (mode[0] == 'w' && mode[1] != '+' && access(filename, 0) == 0)
	unlink(filename);

    /* If the file does not exist, use Create call */
    if (access(filename, 0) != 0) {
	/* Cannot open file for reading if it does not exist */
	if (mode[0] == 'r') {
	    /* Make it runtime error for now. Later change to ** setting
	       the status variable (how?) and gracefully leaving. */
	    sprintf(ErrorBuffer, "file '%s' does not exist", filename);
	    _RuntimeError(ErrorBuffer);
	}
	Fields = _SigToFields(sig, &nFields);
	if ((*fp = DBF_Create(filename, nFields, Fields)) == NULL) {
	    sprintf(ErrorBuffer, "could not create data file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
    }
    else {
	if ((*fp = DBF_Open(filename, mode)) == NULL) {
	    sprintf(ErrorBuffer, "could not open file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
    }
}

void _OpenIndexedFile(DBF_FILE ** fp, char *filename,
		      char *sig, char *mode)
{
char ErrorBuffer[80];
DBF_Field *Fields;
int nFields;

    /* If opening for OUTPUT, remove the file first */
    if (mode[0] == 'w' && mode[1] != '+' && access(filename, 0) == 0)
	unlink(filename);

    /* If the file does not exist, use Create call */
    if (access(filename, 0) != 0) {
	/* Cannot open file for reading if it does not exist */
	if (mode[0] == 'r') {
	    /* Make it runtime error for now. Later change to ** setting
	       the status variable (how?) and gracefully leaving. */
	    sprintf(ErrorBuffer, "file '%s' does not exist", filename);
	    _RuntimeError(ErrorBuffer);
	}
	Fields = _SigToFields(sig, &nFields);
	if ((*fp = DBF_Create(filename, nFields, Fields)) == NULL) {
	    sprintf(ErrorBuffer, "could not create data file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
    }
    else {
	if ((*fp = DBF_Open(filename, mode)) == NULL) {
	    sprintf(ErrorBuffer, "could not open data file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
    }
}

void _CloseSequentialFile(FILE * fp)
{
    fclose(fp);
}

void _CloseRelativeFile(DBF_FILE ** fp)
{
    DBF_Close(*fp);
    *fp = NULL;
}

void _CloseIndexedFile(DBF_FILE ** fpd)
{
    DBF_Close(*fpd);
    *fpd = NULL;
}

void _SeekRelativeFile(DBF_FILE * fp, long int record)
{
    DBF_SeekToRecord(fp, record);
}

void _SeekIndexedFile(DBF_FILE * fp, NDX_FILE * fpd, char *key)
{
int index;
    index = NDX_LookupKey (fpd, key);
    if (index >= 0)
	DBF_SeekToRecord(fp, index);
}

void _OpenIndexFile(NDX_FILE ** ifd, char *filename,
		    char *sig, char *mode)
{
char ErrorBuffer[80];
DBF_Field *Fields;
int nFields;

    if (mode[0] == 'w' && access (filename,0) == 0)
	unlink (filename);

    /* If the file does not exist, use Create call */
    if (access(filename, 0) != 0) {
	/* Cannot open file for reading if it does not exist */
	if (mode[0] == 'r') {
	    /* Make it runtime error for now. Later change to 
	    ** setting the status variable (how?) and gracefully leaving. 
	    */
	    sprintf(ErrorBuffer, "file '%s' does not exist", filename);
	    _RuntimeError(ErrorBuffer);
	}
	Fields = _SigToFields (sig, &nFields);
	if ((*ifd = NDX_Create (filename, Fields->Name, 
	    	Fields->Type, Fields->FieldLength)) == NULL) 
	{
	    sprintf(ErrorBuffer, "could not create data file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
	free (Fields);
    }
    else {
	if ((*ifd = NDX_Open(filename, mode)) == NULL) {
	    sprintf(ErrorBuffer, "could not open data file '%s'", filename);
	    _RuntimeError(ErrorBuffer);
	}
    }
}

void _CloseIndexFile(NDX_FILE ** ifd)
{
    NDX_Close (ifd);
}
