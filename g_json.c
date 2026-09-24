/**
 * <https://github.com/rafagafe/tiny-json>
 *
 * Licensed under the MIT License <http://opensource.org/licenses/MIT>.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2016-2018 Rafa Garcia <rafagarcia77@gmail.com>.
 *
 * Permission is hereby  granted, free of charge, to any  person obtaining a copy
 * of this software and associated  documentation files (the "Software"), to deal
 * in the Software  without restriction, including without  limitation the rights
 * to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
 * copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
 * IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
 * FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
 * AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
 * LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "g_local.h"
#include "g_json.h"

/** Structure to handle a heap of JSON properties. */
typedef struct jsonStaticPool_s {
    json_t* mem;      /**< Pointer to array of json properties.      */
    unsigned int qty; /**< Length of the array of json properties.   */
    unsigned int nextFree;  /**< The index of the next free json property. */
    jsonPool_t pool;
} jsonStaticPool_t;

/** 
 * Search a property by its name in a JSON object.
    * Properties are held as a linked list of siblings rather than anything
    * indexed, so this is a linear walk - fine for the small API responses
    * q2admin parses, but it means looking up n fields costs n walks.
    * @param obj The object to search. Must be a JSON_OBJ or JSON_ARRAY;
    *            passing a leaf property reads a union member that isn't
    *            the child pointer.
    * @param property Name to look for, matched case sensitively.
    * @retval The property if found. Use json_getValue()/json_getBoolean()/
    *         json_getReal()/json_getInteger() to read it, or pass it back
    *         here to descend into a nested object.
    * @retval Null pointer if there is no such property.
    * Called from the API response parsers in g_vpn.c - both for reading
    * fields directly and for stepping into nested objects. 
    */
json_t const* json_getProperty( json_t const* obj, char const* property ) {
    json_t const* sibling;
    for( sibling = obj->u.c.child; sibling; sibling = sibling->sibling )
        if ( sibling->name && !strcmp( sibling->name, property ) )
            return sibling;
    return 0;
}

/** Search a property by its name in a JSON object and return its value
  * as text, combining the lookup and the read into one call for the
  * common case of pulling a single scalar field out of a response.
  * @param obj The object to search, as json_getProperty().
  * @param property Name to look for, matched case sensitively.
  * @retval Pointer to the value as a string. Numbers and booleans come
  *         back in their original text form ("42", "true"), since the
  *         parser doesn't convert until asked. The pointer is into the
  *         buffer that was parsed, so it's only valid while that buffer
  *         is, and it must not be freed.
  * @retval Null pointer if the property is missing, or if it's an object
  *         or array - those have no scalar value to return, so descend
  *         with json_getProperty() instead.
  * Called from the API response parsers in g_vpn.c for reading string
  * fields out of the vpnapi.io and IPLogs replies. */
char const* json_getPropertyValue( json_t const* obj, char const* property ) {
	json_t const* field = json_getProperty( obj, property );
	if ( !field ) return 0;
        jsonType_t type = json_getType( field );
        if ( JSON_ARRAY >= type ) return 0;
	return json_getValue( field );
}

/* Internal prototypes: */
static char* goBlank( char* str );
static char* goNum( char* str );
static json_t* poolInit( jsonPool_t* pool );
static json_t* poolAlloc( jsonPool_t* pool );
static char* objValue( char* ptr, json_t* obj, jsonPool_t* pool );
static char* setToNull( char* ch );
static bool isEndOfPrimitive( char ch );

/** Parse a string to get a json, taking properties from a caller-supplied
  * pool. The generic form behind json_create(): the pool is an interface
  * of init/alloc callbacks, so where the json_t records come from is the
  * caller's choice rather than this library's.
  * @param str The string to parse. Modified in place - see json_create().
  * @param pool Pool providing the json_t instances.
  * @retval The root object if success. Its type is JSON_OBJ or
  *         JSON_ARRAY depending on which the text started with.
  * @retval Null pointer if the text isn't valid JSON, doesn't start with
  *         '{' or '[', or the pool ran out of properties.
  * Called from json_create() below; q2admin has no other pool
  * implementation, so that's currently the only caller. */
json_t const* json_createWithPool( char *str, jsonPool_t *pool ) {
    char* ptr = goBlank( str );
    if ( !ptr || (*ptr != '{' && *ptr != '[') ) return 0;
    json_t* obj = pool->init( pool );
    obj->name    = 0;
    obj->sibling = 0;
    obj->u.c.child = 0;
    ptr = objValue( ptr, obj, pool );
    if ( !ptr ) return 0;
    return obj;
}

/** Parse a string to get a json. The entry point q2admin uses, and the
  * one place the library's two defining constraints show up, both of
  * which are why it suits a game server: it never allocates, and it
  * doesn't copy.
  *
  * No allocation - every property comes out of the caller's `mem` array,
  * so memory use is bounded and decided up front by the caller. Running
  * out just fails the parse rather than growing.
  *
  * No copying - values point straight into `str`, which is rewritten in
  * place as it parses (terminators written over closing quotes and
  * delimiters). Two consequences worth being careful about: `str` must
  * stay alive and untouched for as long as the returned tree is read
  * from, since the tree is largely pointers into it, and `str` is
  * destroyed by the parse, so keep a copy if the raw text is still
  * needed.
  *
  * @param str The string to parse, modified in place as described.
  * @param mem Caller-owned array of json_t to build the tree in. Needs
  *            one entry for the root plus one per property anywhere in
  *            the document, nested ones included.
  * @param qty Number of entries in mem.
  * @retval The root object if success.
  * @retval Null pointer if the text isn't valid JSON or mem was too
  *         small - the two are indistinguishable, so a parse failure on
  *         good input is worth suspecting as an undersized mem.
  * Called from the HTTP response handlers in g_vpn.c, which parse the
  * vpnapi.io and IPLogs replies out of a stack buffer. */
json_t const* json_create( char* str, json_t mem[], unsigned int qty ) {
    jsonStaticPool_t spool;
    spool.mem = mem;
    spool.qty = qty;
    spool.pool.init = poolInit;
    spool.pool.alloc = poolAlloc;
    return json_createWithPool( str, &spool.pool );
}

/** Get a special character with its escape character. Examples:
  * 'b' -> '\\b', 'n' -> '\\n', 't' -> '\\t'
  * Table driven rather than a switch so the set of recognised escapes is
  * exactly JSON's, and anything else is rejected rather than silently
  * passed through.
  * @param ch The escape character (the one *after* the backslash).
  * @retval  The character code.
  * @retval  '\0' if ch isn't a valid JSON escape, which callers treat as
  *          a parse error.
  * Called from parseString() below. */
static char getEscape( char ch ) {
    static struct { char ch; char code; } const pair[] = {
        { '\"', '\"' }, { '\\', '\\' },
        { '/',  '/'  }, { 'b',  '\b' },
        { 'f',  '\f' }, { 'n',  '\n' },
        { 'r',  '\r' }, { 't',  '\t' },
    };
    unsigned int i;
    for( i = 0; i < sizeof pair / sizeof *pair; ++i )
        if ( pair[i].ch == ch )
            return pair[i].code;
    return '\0';
}

/** Parse 4 characters.
  * Validates a \\uXXXX escape without actually decoding it: a well-formed
  * one is accepted and substituted with a literal '?'. Since values are
  * rewritten in place and a decoded code point could need more bytes
  * than the escape it replaces, this parser deliberately doesn't do
  * real Unicode - so non-ASCII text survives parsing but arrives as
  * question marks.
  * @param str Pointer to  first digit.
  * @retval '?' If the four characters are hexadecimal digits.
  * @retval '\0' In other cases.
  * Called from parseString() below. */
static unsigned char getCharFromUnicode( unsigned char const* str ) {
    unsigned int i;
    for( i = 0; i < 4; ++i )
        if ( !isxdigit( str[i] ) )
            return '\0';
    return '?';
}

/** Parse a string and replace the scape characters by their meaning characters.
  * This parser stops when finds the character '\"'. Then replaces '\"' by '\0'.
  * Unescaping happens in place using separate read and write pointers -
  * safe because an escape sequence is always at least as long as the one
  * character it decodes to, so the writer can never overtake the reader.
  * That in-place rewrite is what lets the finished value be used
  * directly out of the original buffer with no copy.
  * @param str Pointer to first character.
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur - an unterminated string, or
  *         an invalid escape.
  * Called from propertyName() and textValue() below, i.e. for both
  * halves of a "name": "value" pair. */
static char* parseString( char* str ) {
    unsigned char* head = (unsigned char*)str;
    unsigned char* tail = (unsigned char*)str;
    for( ; *head; ++head, ++tail ) {
        if ( *head == '\"' ) {
            *tail = '\0';
            return (char*)++head;
        }
        if ( *head == '\\' ) {
            if ( *++head == 'u' ) {
                char const ch = getCharFromUnicode( ++head );
                if ( ch == '\0' ) return 0;
                *tail = ch;
                head += 3;
            }
            else {
                char const esc = getEscape( *head );
                if ( esc == '\0' ) return 0;
                *tail = esc;
            }
        }
        else *tail = *head;
    }
    return 0;
}

/** Parse a string to get the name of a property.
  * Handles the whole `"name" :` part of a member, name through separator,
  * leaving the caller positioned on the value. The name is pointed at
  * in place rather than copied, which is what json_getProperty() later
  * strcmp()s against.
  * @param ptr Pointer to first character (the opening quote).
  * @param property The property to assign the name.
  * @retval Pointer to first of property value. If success.
  * @retval Null pointer if any error occur - a malformed name, or a
  *         missing ':'.
  * Called from objValue() below, for each member of an object (but not
  * of an array, whose elements have no names). */
static char* propertyName( char* ptr, json_t* property ) {
    property->name = ++ptr;
    ptr = parseString( ptr );
    if ( !ptr ) return 0;
    ptr = goBlank( ptr );
    if ( !ptr ) return 0;
    if ( *ptr++ != ':' ) return 0;
    return goBlank( ptr );
}

/** Parse a string to get the value of a property when its type is JSON_TEXT.
  * The caller has already pointed property->u.value at the opening
  * quote, so the pre-increment here steps it past that to the first real
  * character - the value ends up referring to the unescaped text
  * parseString() leaves behind, in place.
  * @param ptr Pointer to first character ('\"').
  * @param property The property to assign the name.
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from objValue() below when a value starts with a quote. */
static char* textValue( char* ptr, json_t* property ) {
    ++property->u.value;
    ptr = parseString( ++ptr );
    if ( !ptr ) return 0;
    property->type = JSON_TEXT;
    return ptr;
}

/** Compare two strings until get the null character in the second one.
  * A prefix match that reports where it stopped rather than just whether
  * it matched, which is what lets the caller keep parsing from the end of
  * a recognised literal.
  * @param ptr sub string
  * @param str main string
  * @retval Pointer to next character.
  * @retval Null pointer if any error occur, i.e. the strings diverged
  *         before str ran out.
  * Called from primitiveValue() below to match "true"/"false"/"null". */
static char* checkStr( char* ptr, char const* str ) {
    while( *str )
        if ( *ptr++ != *str++ )
            return 0;
    return ptr;
}

/** Parser a string to get a primitive value.
  * If the first character after the value is different of '}' or ']' is set to '\0'.
  * Shared body for the three keyword literals. Requiring a proper
  * terminator after the word is what stops "truex" or "nullish" being
  * accepted as their prefixes.
  * @param ptr Pointer to first character.
  * @param property Property handler to set the value and the type, (true, false or null).
  * @param value String with the primitive literal.
  * @param type The code of the type. ( JSON_BOOLEAN or JSON_NULL )
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from trueValue(), falseValue() and nullValue() below. */
static char* primitiveValue( char* ptr, json_t* property, char const* value, jsonType_t type ) {
    ptr = checkStr( ptr, value );
    if ( !ptr || !isEndOfPrimitive( *ptr ) ) return 0;
    ptr = setToNull( ptr );
    property->type = type;
    return ptr;
}

/** Parser a string to get a true value.
  * If the first character after the value is different of '}' or ']' is set to '\0'.
  * @param ptr Pointer to first character.
  * @param property Property handler to set the value and the type, (true, false or null).
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from objValue() below when a value starts with 't'. The type
  * is recorded as JSON_BOOLEAN; which of the two it is gets worked out
  * later by json_getBoolean() reading back the stored text. */
static char* trueValue( char* ptr, json_t* property ) {
    return primitiveValue( ptr, property, "true", JSON_BOOLEAN );
}

/** Parser a string to get a false value.
  * If the first character after the value is different of '}' or ']' is set to '\0'.
  * @param ptr Pointer to first character.
  * @param property Property handler to set the value and the type, (true, false or null).
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from objValue() below when a value starts with 'f'. */
static char* falseValue( char* ptr, json_t* property ) {
    return primitiveValue( ptr, property, "false", JSON_BOOLEAN );
}

/** Parser a string to get a null value.
  * If the first character after the value is different of '}' or ']' is set to '\0'.
  * @param ptr Pointer to first character.
  * @param property Property handler to set the value and the type, (true, false or null).
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from objValue() below when a value starts with 'n'. A JSON
  * null is kept as a real property of type JSON_NULL rather than being
  * dropped, so callers can tell "present but null" from "absent". */
static char* nullValue( char* ptr, json_t* property ) {
    return primitiveValue( ptr, property, "null", JSON_NULL );
}

/** Analyze the exponential part of a real number.
  * Validates the part after 'e'/'E' - an optional sign then at least one
  * digit. Only checks the shape; nothing is converted to a number here.
  * @param ptr Pointer to first character (just past the 'e'/'E').
  * @retval Pointer to first non numerical after the string. If success.
  * @retval Null pointer if any error occur, i.e. an exponent with no
  *         digits, or digits running to the end of the buffer.
  * Called from numValue() below. */
static char* expValue( char* ptr ) {
    if ( *ptr == '-' || *ptr == '+' ) ++ptr;
    if ( !isdigit( (int)(*ptr) ) ) return 0;
    ptr = goNum( ++ptr );
    return ptr;
}

/** Analyze the decimal part of a real number.
  * Validates the part after the '.', which JSON requires to have at
  * least one digit - "1." is not a number.
  * @param ptr Pointer to first character (just past the '.').
  * @retval Pointer to first non numerical after the string. If success.
  * @retval Null pointer if any error occur.
  * Called from numValue() below. */
static char* fraqValue( char* ptr ) {
    if ( !isdigit( (int)(*ptr) ) ) return 0;
    ptr = goNum( ++ptr );
    if ( !ptr ) return 0;
    return ptr;
}

/** Parser a string to get a numerical value.
  * If the first character after the value is different of '}' or ']' is set to '\0'.
  * Also the fallback for anything that isn't a string, keyword, object or
  * array, so this is where malformed values are finally rejected.
  *
  * Enforces JSON's number rules strictly - no leading '+', no leading
  * zeros, a digit required either side of a '.' - and classifies as
  * JSON_INTEGER unless a fraction or exponent appears, which promotes it
  * to JSON_REAL. Integers are additionally range checked against
  * int64_t by *string* comparison against the min/max literals, since
  * the value isn't converted here and converting first is exactly what
  * would overflow.
  * @param ptr Pointer to first character.
  * @param property Property handler to set the value and the type: JSON_REAL or JSON_INTEGER.
  * @retval Pointer to first non white space after the string. If success.
  * @retval Null pointer if any error occur, including an integer too
  *         large to hold.
  * Called from objValue() below as the default case for a value. */
static char* numValue( char* ptr, json_t* property ) {
    if ( *ptr == '-' ) ++ptr;
    if ( !isdigit( (int)(*ptr) ) ) return 0;
    if ( *ptr != '0' ) {
        ptr = goNum( ptr );
        if ( !ptr ) return 0;
    }
    else if ( isdigit( (int)(*++ptr) ) ) return 0;
    property->type = JSON_INTEGER;
    if ( *ptr == '.' ) {
        ptr = fraqValue( ++ptr );
        if ( !ptr ) return 0;
        property->type = JSON_REAL;
    }
    if ( *ptr == 'e' || *ptr == 'E' ) {
        ptr = expValue( ++ptr );
        if ( !ptr ) return 0;
        property->type = JSON_REAL;
    }
    if ( !isEndOfPrimitive( *ptr ) ) return 0;
    if ( JSON_INTEGER == property->type ) {
        char const* value = property->u.value;
        bool const negative = *value == '-';
        static char const min[] = "-9223372036854775808";
        static char const max[] = "9223372036854775807";
        unsigned int const maxdigits = ( negative? sizeof min: sizeof max ) - 1;
        unsigned int const len = ( unsigned int const ) ( ptr - value );
        if ( len > maxdigits ) return 0;
        if ( len == maxdigits ) {
            char const tmp = *ptr;
            *ptr = '\0';
            char const* const threshold = negative ? min: max;
            if ( 0 > strcmp( threshold, value ) ) return 0;
            *ptr = tmp;
        }
    }
    ptr = setToNull( ptr );
    return ptr;
}

/** Add a property to a JSON object or array.
  * Appends to the parent's singly linked child list. Keeping a
  * last_child pointer alongside the head is what makes this O(1) - the
  * alternative, walking to the tail each time, would make parsing a
  * large object quadratic. Document order is preserved as a result.
  * @param obj The handler of the JSON object or array.
  * @param property The handler of the property to be added.
  * Returns nothing.
  * Called from objValue() below, once per property parsed. */
static void add( json_t* obj, json_t* property ) {
    property->sibling = 0;
    if ( !obj->u.c.child ){
	    obj->u.c.child = property;
	    obj->u.c.last_child = property;
    } else {
	    obj->u.c.last_child->sibling = property;
	    obj->u.c.last_child = property;
    }
}

/** Parser a string to get a json object value.
  * The main parse loop, and the one function here that handles the
  * document as a whole rather than one token - everything else above is
  * a helper it dispatches to based on the first character of a value.
  *
  * Nesting is handled iteratively rather than by recursing: on '{' or
  * '[' it borrows the new property's `sibling` field to point back at
  * the current parent, descends, and follows that link back up on the
  * closing brace, rewriting sibling to its real value on the way out.
  * That keeps stack depth constant no matter how deeply nested the
  * document is, which matters when the input is a remote API response
  * of unknown shape.
  * @param ptr Pointer to first character.
  * @param obj The handler of the JSON root object or array.
  * @param pool The handler of a json pool for creating json instances.
  * @retval Pointer to first character after the value. If success.
  * @retval Null pointer if any error occur, including the pool running
  *         out of properties mid-document.
  * Called from json_createWithPool() above, once for the whole document.
  *
  * Note it's lenient about commas - separators are skipped wherever they
  * appear rather than being required exactly between values, so some
  * malformed input parses without complaint. */
static char* objValue( char* ptr, json_t* obj, jsonPool_t* pool ) {
    obj->type    = *ptr == '{' ? JSON_OBJ : JSON_ARRAY;
    obj->u.c.child = 0;
    obj->sibling = 0;
    ptr++;
    for(;;) {
        ptr = goBlank( ptr );
        if ( !ptr ) return 0;
        if ( *ptr == ',' ) {
            ++ptr;
            continue;
        }
        char const endchar = ( obj->type == JSON_OBJ )? '}': ']';
        if ( *ptr == endchar ) {
            *ptr = '\0';
            json_t* parentObj = obj->sibling;
            if ( !parentObj ) return ++ptr;
            obj->sibling = 0;
            obj = parentObj;
            ++ptr;
            continue;
        }
        json_t* property = pool->alloc( pool );
        if ( !property ) return 0;
        if( obj->type != JSON_ARRAY ) {
            if ( *ptr != '\"' ) return 0;
            ptr = propertyName( ptr, property );
            if ( !ptr ) return 0;
        }
        else property->name = 0;
        add( obj, property );
        property->u.value = ptr;
        switch( *ptr ) {
            case '{':
                property->type    = JSON_OBJ;
                property->u.c.child = 0;
                property->sibling = obj;
                obj = property;
                ++ptr;
                break;
            case '[':
                property->type    = JSON_ARRAY;
                property->u.c.child = 0;
                property->sibling = obj;
                obj = property;
                ++ptr;
                break;
            case '\"': ptr = textValue( ptr, property );  break;
            case 't':  ptr = trueValue( ptr, property );  break;
            case 'f':  ptr = falseValue( ptr, property ); break;
            case 'n':  ptr = nullValue( ptr, property );  break;
            default:   ptr = numValue( ptr, property );   break;
        }
        if ( !ptr ) return 0;
    }
}

/** Initialize a json pool.
  * The static pool's implementation of the pool interface's init hook.
  * Hands back mem[0] for the root and marks everything after it free.
  * Recovers the enclosing jsonStaticPool_t from the embedded pool member
  * via json_containerOf(), which is how this gets at mem/qty through an
  * interface pointer that knows nothing about them.
  * @param pool The handler of the pool.
  * @return a instance of a json - always mem[0], the root.
  * Called through the pool interface from json_createWithPool(), for
  * pools set up by json_create(). */
static json_t* poolInit( jsonPool_t* pool ) {
    jsonStaticPool_t *spool = json_containerOf( pool, jsonStaticPool_t, pool );
    spool->nextFree = 1;
    return spool->mem;
}

/** Create an instance of a json from a pool.
  * The static pool's implementation of the pool interface's alloc hook,
  * and the whole of this library's memory management: hand out the next
  * slot of the caller's array and move on. Nothing is ever freed or
  * reused, since the tree lives exactly as long as the array does.
  * @param pool The handler of the pool.
  * @retval The handler of the new instance if success.
  * @retval Null pointer if the pool was empty, which fails the parse -
  *         so an array sized too small for the document looks the same
  *         to the caller as malformed JSON.
  * Called through the pool interface from objValue(), once per property. */
static json_t* poolAlloc( jsonPool_t* pool ) {
    jsonStaticPool_t *spool = json_containerOf( pool, jsonStaticPool_t, pool );
    if ( spool->nextFree >= spool->qty ) return 0;
    return spool->mem + spool->nextFree++;
}

/** Checks whether an character belongs to set.
  * Character sets are plain strings here rather than lookup tables,
  * keeping the library free of static data beyond a couple of short
  * literals.
  * @param ch Character value to be checked.
  * @param set Set of characters. It is just a null-terminated string.
  * @return true or false there is membership or not.
  * Called from goWhile(), setToNull() and isEndOfPrimitive() below. */
static bool isOneOfThem( char ch, char const* set ) {
    while( *set != '\0' )
        if ( ch == *set++ )
            return true;
    return false;
}

/** Increases a pointer while it points to a character that belongs to a set.
  * Generic "skip over these characters" scan. Note that reaching the end
  * of the buffer is reported as failure rather than success - running out
  * of input mid-document always means truncated JSON, so this doubles as
  * the bounds check that keeps the parser inside the string.
  * @param str The initial pointer value.
  * @param set Set of characters. It is just a null-terminated string.
  * @return The final pointer value or null pointer if the null character was found.
  * Called from goBlank() below. */
static char* goWhile( char* str, char const* set ) {
    for(; *str != '\0'; ++str ) {
        if ( !isOneOfThem( *str, set ) )
            return str;
    }
    return 0;
}

/** Set of characters that defines a blank. */
static char const* const blank = " \n\r\t\f";

/** Increases a pointer while it points to a white space character.
  * Skips the insignificant whitespace JSON allows between any two
  * tokens; called at essentially every point the parser expects
  * something new.
  * @param str The initial pointer value.
  * @return The final pointer value or null pointer if the null character was found.
  * Called from json_createWithPool(), propertyName() and objValue(). */
static char* goBlank( char* str ) {
    return goWhile( str, blank );
}

/** Increases a pointer while it points to a decimal digit character.
  * Runs off the end of a digit run, leaving the caller on whatever
  * followed it. Doesn't use goWhile() since the set is a character class
  * rather than a literal string.
  * @param str The initial pointer value.
  * @return The final pointer value or null pointer if the null character was found.
  * Called from numValue(), fraqValue() and expValue() above. */
static char* goNum( char* str ) {
    for( ; *str != '\0'; ++str ) {
        if ( !isdigit( (int)(*str) ) )
            return str;
    }
    return 0;
}

/** Set of characters that defines the end of an array or a JSON object. */
static char const* const endofblock = "}]";

/** Set a char to '\0' and increase its pointer if the char is different to '}' or ']'.
  * How a primitive value gets terminated in place: the delimiter that
  * ended it is simply overwritten, turning the value into a standalone
  * C string with no copying. A closing brace or bracket is left intact
  * instead, because objValue() still needs to see it to know the
  * object or array ended - it terminates those itself once it has.
  * @param ch Pointer to character.
  * @return  Final value pointer.
  * Called from primitiveValue() and numValue() above. */
static char* setToNull( char* ch ) {
    if ( !isOneOfThem( *ch, endofblock ) ) *ch++ = '\0';
    return ch;
}

/** Indicate if a character is the end of a primitive value.
  * The check that keeps a bare value from swallowing what follows it: a
  * number or keyword may only be followed by a separator, whitespace or
  * the end of its container. Anything else means the token was malformed
  * (e.g. "truex", "12abc") and the parse fails.
  * @param ch Character following the value just parsed.
  * @return true if it validly ends a primitive, false otherwise.
  * Called from primitiveValue() and numValue() above. */
static bool isEndOfPrimitive( char ch ) {
    return ch == ',' || isOneOfThem( ch, blank ) || isOneOfThem( ch, endofblock );
}

