// cvar.c -- dynamic variable tracking

#include "../game/q_shared.h"
#include "qcommon.h"
#include <vector>
#include <algorithm>

typedef std::vector<cvar_t *> cvarvec_t;

cvar_t		*cvar_vars;
cvar_t		*cvar_cheats;
int			cvar_modifiedFlags;

#define	MAX_CVARS	2048
cvar_t		cvar_indexes[MAX_CVARS];
int			cvar_numIndexes;

#define FILE_HASH_SIZE		256
static	cvar_t*		hashTable[FILE_HASH_SIZE];

cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force);

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

/*
============
Cvar_ValidateString
============
*/
static qboolean Cvar_ValidateString( const char *s ) {
	if ( !s ) {
		return qfalse;
	}
	if ( strchr( s, '\\' ) ) {
		return qfalse;
	}
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t	*var;
	long hash;

	hash = generateHashValue(var_name);
	
	for (var=hashTable[hash] ; var ; var=var->hashNext) {
		if (!Q_stricmp(var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->value;
}


/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->integer;
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString( const char *var_name ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var) {
		*buffer = 0;
	}
	else {
		Q_strncpyz( buffer, var->string, bufsize );
	}
}


/*
============
Cvar_CommandCompletion
============
*/
void	Cvar_CommandCompletion( void(*callback)(const char *s) ) {
	cvar_t		*cvar;
	
	for ( cvar = cvar_vars ; cvar ; cvar = cvar->next ) {
		// Dont show internal cvars
		if ( cvar->flags & CVAR_INTERNAL )
		{
			continue;
		}
		callback( cvar->name );
	}
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	cvar_t	*var;
	long	hash;

    if ( !var_name || ! var_value ) {
		Com_Error( ERR_FATAL, "Cvar_Get: NULL parameter" );
    }

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0		// FIXME: values with backslash happen
	if ( !Cvar_ValidateString( var_value ) ) {
		Com_Printf("invalid cvar value string: %s\n", var_value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar (var_name);
	if ( var ) {
		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value
		if ( ( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED )
			&& var_value[0] ) {
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );

			// ZOID--needs to be set so that cvars the game sets as 
			// SERVERINFO get sent to clients
			int tmpFlags = flags;
			if (com_skipWrite->integer) {
				tmpFlags &= ~CVAR_ARCHIVE;
			}
			cvar_modifiedFlags |= tmpFlags;
		}

		var->flags |= flags;
		// only allow one non-empty reset string without a warning
		if ( !var->resetString[0] ) {
			// we don't have a reset string yet
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );
		} else if ( var_value[0] && strcmp( var->resetString, var_value ) ) {
			Com_DPrintf( "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
				var_name, var->resetString, var_value );
		}
		// if we have a latched string, take that value now
		if ( var->latchedString ) {
			char *s;

			s = var->latchedString;
			var->latchedString = NULL;	// otherwise cvar_set2 would free it
			Cvar_Set2( var_name, s, qtrue );
			Z_Free( s );
		}

// use a CVAR_SET for rom sets, get won't override
#if 0
		// CVAR_ROM always overrides
		if ( flags & CVAR_ROM ) {
			Cvar_Set2( var_name, var_value, qtrue );
		}
#endif
		return var;
	}

	//
	// allocate a new cvar
	//
	if ( cvar_numIndexes >= MAX_CVARS ) {
		Com_Error( ERR_FATAL, "MAX_CVARS" );
	}
	var = &cvar_indexes[cvar_numIndexes];
	cvar_numIndexes++;
	var->name = CopyString (var_name);
	var->string = CopyString (var_value);
	var->modified = qtrue;
	var->modificationCount = 1;
	var->value = atof (var->string);
	var->integer = atoi(var->string);
	var->resetString = CopyString( var_value );

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	hash = generateHashValue(var_name);
	var->hashNext = hashTable[hash];
	hashTable[hash] = var;

	return var;
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ) {
	cvar_t	*var;

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0	// FIXME
	if ( value && !Cvar_ValidateString( value ) ) {
		Com_Printf("invalid cvar value string: %s\n", value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar (var_name);
	if (!var) {
		if ( !value ) {
			return NULL;
		}
		// create it
		if ( !force ) {
			return Cvar_Get( var_name, value, CVAR_USER_CREATED );
		} else {
			return Cvar_Get (var_name, value, 0);
		}
	}

	// Dont display the update when its internal
	if ( !(var->flags & CVAR_INTERNAL) )
	{
		Com_DPrintf( "Cvar_Set2: %s %s\n", var_name, value );
	}

	if (!value ) {
		value = var->resetString;
	}

	if (!strcmp(value,var->string)) {
		return var;
	}
	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	int tmpFlags = var->flags;
	if (com_skipWrite->integer) {
		tmpFlags &= ~CVAR_ARCHIVE;
	}
	cvar_modifiedFlags |= tmpFlags;

	if (!force)
	{
		if (var->flags & CVAR_ROM)
		{
			Com_Printf ("%s is read only.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_INIT)
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latchedString)
			{
				if (strcmp(value, var->latchedString) == 0)
					return var;
				Z_Free (var->latchedString);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			Com_Printf ("%s will be changed upon restarting.\n", var_name);
			var->latchedString = CopyString(value);
			var->modified = qtrue;
			var->modificationCount++;
			return var;
		}

		if ( (var->flags & CVAR_CHEAT) && !cvar_cheats->integer )
		{
			Com_Printf ("%s is cheat protected.\n", var_name);
			return var;
		}

	}
	else
	{
		if (var->latchedString)
		{
			Z_Free (var->latchedString);
			var->latchedString = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = qtrue;
	var->modificationCount++;
	
	Z_Free (var->string);	// free the old value string
	
	var->string = CopyString(value);
	var->value = atof (var->string);
	var->integer = atoi (var->string);

	return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qtrue);
}

/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qfalse);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value) {
	char	val[32];

	if ( value == (int)value ) {
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	} else {
		Com_sprintf (val, sizeof(val), "%f",value);
	}
	Cvar_Set (var_name, val);
}


/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name ) {
	Cvar_Set2( var_name, NULL, qfalse );
}


/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void ) {
	cvar_t	*var;

	// set all default vars to the safe value
	for ( var = cvar_vars ; var ; var = var->next ) {
		if ( var->flags & CVAR_CHEAT ) {
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
			// because of a different var->latchedString
			if (var->latchedString)
			{
				Z_Free(var->latchedString);
				var->latchedString = NULL;
			}
			if (strcmp(var->resetString,var->string)) {
				Cvar_Set( var->name, var->resetString );
			}
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command( void ) {
	cvar_t			*v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) {
		return qfalse;
	}

	// perform a variable print or set
	if ( Cmd_Argc() == 1 ) 
	{
/*		if (v->flags & CVAR_INTERNAL) // don't display
		{
			return qtrue;
		}
*/
		Com_Printf ("\"%s\" is:\"%s" S_COLOR_WHITE "\" default:\"%s" S_COLOR_WHITE "\"\n", v->name, v->string, v->resetString );
		if ( v->latchedString ) {
			Com_Printf( "latched: \"%s\"\n", v->latchedString );
		}
		return qtrue;
	}

//JFM toggle test
	char *value;
	value = Cmd_Argv(1);
	if (value[0] =='!')	//toggle
	{
		char buff[5];
		sprintf(buff,"%i",!v->value);
		Cvar_Set2 (v->name, buff, qfalse);// toggle the value
	}
	else
	{
		Cvar_Set2 (v->name, value, qfalse);// set the value if forcing isn't required
	}
	
	return qtrue;
}


/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding
============
*/
void Cvar_Toggle_f( void ) {
	int		v;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: toggle <variable>\n");
		return;
	}

	v = Cvar_VariableValue( Cmd_Argv( 1 ) );
	v = !v;

	Cvar_Set2 (Cmd_Argv(1), va("%i", v), qfalse);
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f( void ) {
	int		i, c, l, len;
	char	combined[MAX_STRING_TOKENS];

	c = Cmd_Argc();
	if ( c < 3 ) {
		Com_Printf ("usage: set <variable> <value>\n");
		return;
	}

	combined[0] = 0;
	l = 0;
	for ( i = 2 ; i < c ; i++ ) {
		len = strlen ( Cmd_Argv( i ) + 1 );
		if ( l + len >= MAX_STRING_TOKENS - 2 ) {
			break;
		}
		strcat( combined, Cmd_Argv( i ) );
		if ( i != c-1 ) {
			strcat( combined, " " );
		}
		l += len;
	}
	Cvar_Set2 (Cmd_Argv(1), combined, qfalse);
}

/*
============
Cvar_SetU_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetU_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: setu <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_USERINFO;
}

/*
============
Cvar_SetS_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetS_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: sets <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_SERVERINFO;
}

/*
============
Cvar_SetA_f

As Cvar_Set, but also flags it as archived
============
*/
void Cvar_SetA_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: seta <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_ARCHIVE;
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}

bool CvarSort(const cvar_t *cv1, const cvar_t *cv2) {
	return Q_stricmp(cv1->name, cv2->name) < 0;
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to qtrue.
============
*/
void Cvar_WriteVariables( fileHandle_t f ) {
/*	cvar_t	*var;
	char	buffer[1024];

	for (var = cvar_vars ; var ; var = var->next) {
#ifdef USE_CD_KEY
		if( Q_stricmp( var->name, "cl_cdkey" ) == 0 ) {
			continue;
		}
#endif // USE_CD_KEY
		if( var->flags & CVAR_ARCHIVE ) {
			// write the latched value, even if it hasn't taken effect yet
			if ( var->latchedString ) {
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->latchedString);
			} else {
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
			}
			FS_Printf (f, "%s", buffer);
		}
	}
*/	cvarvec_t cvar_vec;
	for (cvar_t *var = cvar_vars ; var ; var = var->next) {
		if( !var->name )
			continue;

		if( var->flags & CVAR_ARCHIVE ) {
			cvar_vec.push_back(var);
		}
	}

	std::sort(cvar_vec.begin(), cvar_vec.end(), CvarSort);

	cvarvec_t::const_iterator itr;
	char buffer[1024];
	for (itr = cvar_vec.begin(); itr != cvar_vec.end(); ++itr)
	{
		// write the latched value, even if it hasn't taken effect yet
		if ( (*itr)->latchedString ) {
			if( strlen( (*itr)->name ) + strlen( (*itr)->latchedString ) + 10 > sizeof( buffer ) ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
						"\"%s\" too long to write to file\n", (*itr)->name );
				continue;
			}
			Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", (*itr)->name, (*itr)->latchedString);
		} else {
			if( strlen( (*itr)->name ) + strlen( (*itr)->string ) + 10 > sizeof( buffer ) ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
						"\"%s\" too long to write to file\n", (*itr)->name );
				continue;
			}
			Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", (*itr)->name, (*itr)->string);
		}
		FS_Write( buffer, strlen( buffer ), f );
	}
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f( void ) {
	cvar_t	*var;
	int		i;
	char	*match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	for (var = cvar_vars ; var ; var = var->next, i++)
	{
		// Dont show internal cvars
		if ( var->flags & CVAR_INTERNAL )
		{
			continue;
		}

		if (match && !Com_Filter(match, var->name, qfalse)) continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Printf("S");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Printf("U");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Printf("R");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Printf("I");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Printf("A");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Printf("L");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Printf("C");
		} else {
			Com_Printf(" ");
		}

		Com_Printf (" %s \"%s\"\n", var->name, var->string);
	}

	Com_Printf ("\n%i total cvars\n", i);
	Com_Printf ("%i cvar indexes\n", cvar_numIndexes);
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f( void ) {
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;
	while ( 1 ) {
		var = *prev;
		if ( !var ) {
			break;
		}

		// don't mess with rom values, or some inter-module
		// communication will get broken (com_cl_running, etc)
		if ( var->flags & ( CVAR_ROM | CVAR_INIT | CVAR_NORESTART ) ) {
			prev = &var->next;
			continue;
		}

		// throw out any variables the user created
		if ( var->flags & CVAR_USER_CREATED ) {
			*prev = var->next;
			if ( var->name ) {
				Z_Free( var->name );
			}
			if ( var->string ) {
				Z_Free( var->string );
			}
			if ( var->latchedString ) {
				Z_Free( var->latchedString );
			}
			if ( var->resetString ) {
				Z_Free( var->resetString );
			}
			// clear the var completely, since we
			// can't remove the index from the list
			Com_Memset( var, 0, sizeof( var ) );
			continue;
		}

		Cvar_Set( var->name, var->resetString );

		prev = &var->next;
	}
}



/*
=====================
Cvar_InfoString
=====================
*/
char	*Cvar_InfoString( int bit ) {
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next) {
		if (var->flags & bit) {
			Info_SetValueForKey (info, var->name, var->string);
		}
	}
	return info;
}

/*
=====================
Cvar_InfoString_Big

  handles large info strings ( CS_SYSTEMINFO )
=====================
*/
char	*Cvar_InfoString_Big( int bit ) {
	static char	info[BIG_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next) {
		if (var->flags & bit) {
			Info_SetValueForKey_Big (info, var->name, var->string);
		}
	}
	return info;
}



/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize ) {
	Q_strncpyz(buff,Cvar_InfoString(bit),buffsize);
}

/*
=====================
Cvar_Register

basically a slightly modified Cvar_Get for the interpreted modules
=====================
*/
void	Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	cvar_t	*cv;

	cv = Cvar_Get( varName, defaultValue, flags );
	if ( !vmCvar ) {
		return;
	}
	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;
	Cvar_Update( vmCvar );
}


/*
=====================
Cvar_Register

updates an interpreted modules' version of a cvar
=====================
*/
void	Cvar_Update( vmCvar_t *vmCvar ) {
	cvar_t	*cv = NULL; // bk001129
	assert(vmCvar); // bk

	if ( (unsigned)vmCvar->handle >= cvar_numIndexes ) {
		Com_Error( ERR_DROP, "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;		// variable might have been cleared by a cvar_restart
	}
	vmCvar->modificationCount = cv->modificationCount;
	// bk001129 - mismatches.
	if ( strlen(cv->string)+1 > MAX_CVAR_VALUE_STRING ) 
	  Com_Error( ERR_DROP, "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING",
		     cv->string, 
		     strlen(cv->string), 
		     sizeof(vmCvar->string) );
	// bk001212 - Q_strncpyz guarantees zero padding and dest[MAX_CVAR_VALUE_STRING-1]==0 
	// bk001129 - paranoia. Never trust the destination string.
	// bk001129 - beware, sizeof(char*) is always 4 (for cv->string). 
	//            sizeof(vmCvar->string) always MAX_CVAR_VALUE_STRING
	//Q_strncpyz( vmCvar->string, cv->string, sizeof( vmCvar->string ) ); // id
	Q_strncpyz( vmCvar->string, cv->string,  MAX_CVAR_VALUE_STRING ); 

	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}


/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void) {
	cvar_cheats = Cvar_Get("sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO );

	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("sets", Cvar_SetS_f);
	Cmd_AddCommand ("setu", Cvar_SetU_f);
	Cmd_AddCommand ("seta", Cvar_SetA_f);
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvar_restart", Cvar_Restart_f);
}
