// This file is part of cobcy, a COBOL-to-C compiler.
//
// Copyright (C) 1995-2008 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "semextern.h"
#include "semdecl.h"
#include "semfile.h"
#include "symrec.h"
#include "symvar.h"
#include "symconst.h"

/*---------------------| Globals |------------------------------*/
  static vector<CobolRecord*>	NestedRecordList;
  vector<StackEntry*> 		VarInit;
  CobolRecord * 		ParentRecord = NULL;
  extern vector<CobolFile*>	SFQueue;
/*--------------------------------------------------------------*/

void DeclareRecordLevel (void)
{
enum {
    RecFirst, 
    RecValue = RecFirst, 
    RecPicture, 
    RecName, 
    RecLevel, 
    RecLast
};
typedef StackEntry * StackEntryPtr;
StackEntryPtr entry[4];
bool NoPicture = false;
CobolRecord * NewRec;
CobolVar * NewVar;
struct {
    char	Name [STACK_IDENT_LENGTH];
    char	Picture [STACK_IDENT_LENGTH];
    int		DeclLevel;
} NewSymbol;
int i;

    // Pop the entries off the stack. There are 4 of them.
    for (i = RecFirst; i < RecLast; ++ i) {
       entry[i] = SemStack.back(); SemStack.pop_back();
    }

    // If a default value was given, append the name and the value
    //	to the variable initialization queue, which will be processed
    //	in InitializeVariables()
    if (entry [RecValue]->kind != SE_Null) {
       VarInit.push_back (entry [RecName]);
       VarInit.push_back (entry [RecValue]);
    }

    // Save other variables so that the stack entries can be disposed of ...
    NewSymbol.DeclLevel = entry [RecLevel]->ival;
    strcpy (NewSymbol.Name, entry [RecName]->ident);
    if (entry[RecPicture]->kind != SE_Null)
       strcpy (NewSymbol.Picture, entry [RecPicture]->ident);
    else
       NoPicture = true;

    // ... and dispose of them here. Name and value were appended to 
    //	variable init queue so only delete these
    delete entry[RecPicture];
    delete entry[RecLevel];

    // Establish whether the line is a record or a variable:
    //	criteria is the presence of a picture field.
    if (NoPicture) {
       NewRec = new CobolRecord;
       NewRec->SetName (NewSymbol.Name);
       NewRec->SetDeclLevel (NewSymbol.DeclLevel);

       // Close all finished parent records
       CloseScopeLevels (NewSymbol.DeclLevel);

       // Try setting the parent now.
       if (ParentRecord != NULL) {
	  // If the parent record is still not NULL, it is a parent of this 
	  // line. To keep the child happy, tell it who the parent will be.
	  NewRec->SetParent (ParentRecord->GetName());
	  // Since this is a record, start a new parent scope
	  NestedRecordList.push_back (ParentRecord);
       }
       else {	// No parent = I am parent
	  ParentRecord = NewRec;
	  NewRec->SetParent (NULL);
	  // Skip a line if level 0 record
	  codef << "\n";
       }
       // A record will always open a new scope
       ParentRecord = NewRec;

       // Actually place the symbol in the table
#ifndef NDEBUG
       cout << "Declaring record " << NewSymbol.Name;
       cout << ", dl = " << NewSymbol.DeclLevel << "\n";
#endif
       SymTable[NewSymbol.Name] = NewRec;

       // Generate the actual field declaration
       NewRec->GenDeclareBegin (codef);
       // Record always increases nesting level
       ++ NestingLevel;
    }
    else {
       NewVar = new CobolVar;
       NewVar->SetName (NewSymbol.Name);
       NewVar->SetDeclLevel (NewSymbol.DeclLevel);
       NewVar->SetPicture (NewSymbol.Picture);

       // Close all finished parent records
       CloseScopeLevels (NewSymbol.DeclLevel);

       // Generate the actual field declaration
       NewVar->GenDeclare (codef);

       // If parent is not nobody, tell it about it
       if (ParentRecord != NULL) {
	  // Adopt this variable.
	  NewVar->SetParent (ParentRecord->GetName());
          ParentRecord->AddChild (NewVar);
       }
       else {
	  NewVar->SetParent (NULL);
	  codef << "\n";	// Skip a line if level 0 variable
       }

       // Actually place the symbol in the table
#ifndef NDEBUG
       cout << "Declaring variable " << NewSymbol.Name;
       cout << ", dl = " << NewSymbol.DeclLevel << "\n";
#endif
       SymTable[NewSymbol.Name] = NewVar;
    }
}

void CloseScopeLevels (uint32_t LastLevel)
{
CobolData * ChildRecord;

    // Check if a record before us was finished by checking
    //	the declaration number. If it is less, the record is done.
    while (!NestedRecordList.empty() && ParentRecord != NULL &&
	   ParentRecord->GetDeclLevel() >= LastLevel) 
    {
#ifndef NDEBUG
       cout << "Closing record " << *ParentRecord;
       cout << ", dl = " << ParentRecord->GetDeclLevel() << "\n";
#endif
       -- NestingLevel;
       ParentRecord->GenDeclareEnd (codef);
       ChildRecord = ParentRecord;
       ParentRecord = NestedRecordList.back(); NestedRecordList.pop_back();
       if (ParentRecord != NULL) {
          ChildRecord->SetParent (ParentRecord->GetName());
          ParentRecord->AddChild (ChildRecord);
       }
    }
    // This happens if the queue is empty, but still not leveled
    //	i.e. the new record is on highest level (0). It will have parent NULL
    if (ParentRecord != NULL && ParentRecord->GetDeclLevel() >= LastLevel)
    {
#ifndef NDEBUG
       cout << "Closing top-level record " << *ParentRecord << "\n";
#endif
       -- NestingLevel;
       ParentRecord->GenDeclareEnd (codef);
       ParentRecord = NULL;
    }
}

void InitializeVariables (void)
{
    codef << "void _SetVarValues (void)\n{\n";
    ++ NestingLevel;
    // Initialize predefined variables
    GenIndent();
    codef << "memset (_space_var, ' ', 200);\n";
    GenIndent();
    codef << "_space_var[200] = 0;\n";

    // Initialize user-defined variables
    assert (VarInit.size() % 2 == 0);
    foreach (vector<StackEntry*>::iterator, i, VarInit) {
	StackEntry* VarName = *i;
	StackEntry* VarValue = *++i;
	CobolVar* attr = (CobolVar*) SymTable [VarName->ident];
	if (VarValue->kind == SE_Identifier) {
	    CobolVar* ValueAttr = (CobolVar*) SymTable [VarValue->ident];
	    attr->GenMove (codef, ValueAttr);
	} else {
	    CobolConstant ValueConst;
	    ValueConst = VarValue;
	    attr->GenMove (codef, ValueConst);
	}
	delete VarName;
	delete VarValue;
    }
    VarInit.clear();

    -- NestingLevel;
    GenIndent();
    codef << "}\n\n";
}

// PRINTER IS PRINTER-DISPLAY
void DeclareSpecialName (void)
{
StackEntry * SpName, * DevName;
CobolFile * DevStream;

    SpName = SemStack.back(); SemStack.pop_back();
    DevName = SemStack.back(); SemStack.pop_back();

    DevStream = new CobolFile;
    DevStream->SetName (SpName->ident);

    if (strcmp (DevName->ident, "printer") == 0) {
       DevStream->SetFilename (PRINTER_SPOOL_FILE);
       DevStream->SetFlushCommand (PRINTER_COMMAND);
       DevStream->SetOrganization (ORG_Sequential);
       DevStream->SetAccessMode (AM_Sequential);
       DevStream->SetUnlinkOnClose (true);
    }
    else
       DevStream->SetFilename (DevName->ident);
    
    // Declaration is here because it will not be mentioned again
    DevStream->GenDeclare (codef);

#ifndef NDEBUG
    cout << "DBG: Declaring special name " << SpName->ident << "\n";
#endif
    SymTable[SpName->ident] = DevStream;
    SFQueue.push_back (DevStream);

    delete DevName;
    delete SpName;
}

