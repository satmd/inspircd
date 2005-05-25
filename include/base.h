/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __BASE_H__ 
#define __BASE_H__ 

#include "inspircd_config.h" 
#include <time.h>
#include <map>
#include <string>

typedef void* VoidPointer;
 
/** The base class for all inspircd classes
*/ 
class classbase
{
 public:
 	/** Time that the object was instantiated (used for TS calculation etc)
 	*/
	time_t age;

	/** Constructor,
	 * Sets the object's time
	 */
	classbase() { age = time(NULL); }
	~classbase() { }
};

/** class Extensible is the parent class of many classes such as userrec and chanrec.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class Extensible : public classbase
{
	/** Private data store
	 */
	std::map<std::string,char*> Extension_Items;
	
public:

	/** Extend an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 * @param p This parameter is a pointer to any data you wish to associate with the object
	 *
	 * You must provide a key to store the data as, and a void* to the data (typedef VoidPointer)
	 * The data will be inserted into the map. If the data already exists, you may not insert it
	 * twice, Extensible::Extend will return false in this case.
	 *
	 * @return Returns true on success, false if otherwise
	 */
	bool Extend(std::string key, char* p);

	/** Shrink an Extensible class.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * You must provide a key name. The given key name will be removed from the classes data. If
	 * you provide a nonexistent key (case is important) then the function will return false.
	 *
	 * @return Returns true on success.
	 */
	bool Shrink(std::string key);
	
	/** Get an extension item.
	 *
	 * @param key The key parameter is an arbitary string which identifies the extension data
	 *
	 * @return If you provide a non-existent key name, the function returns NULL, otherwise a pointer to the item referenced by the key is returned.
	 */
	char* GetExt(std::string key);
};

#endif

