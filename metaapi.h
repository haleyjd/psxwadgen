// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
//   Metatables for storage of multiple types of objects in an associative
//   array.
//
//-----------------------------------------------------------------------------

#ifndef METAAPI_H__
#define METAAPI_H__

#include "e_rtti.h"
#include "m_dllist.h"

// METATYPE macro - make a string from a typename
#define METATYPE(t) #t

// enumeration for metaerrno
enum
{
   META_ERR_NOERR,        // 0 is not an error
   META_ERR_NOSUCHOBJECT,
   META_ERR_NOSUCHTYPE,
   META_NUMERRS
};

extern int metaerrno;

class MetaTablePimpl;

//
// MetaObject
//
class MetaObject : public RTTIObject
{
   DECLARE_RTTI_TYPE(MetaObject, RTTIObject)

protected:
   DLListItem<MetaObject> links;     // links by key
   DLListItem<MetaObject> typelinks; // links by type
   const char *key;                  // primary hash key
   const char *type;                 // type hash key
   size_t      keyIdx;               // index of interned key

   // For efficiency, we are friends with the private implementation
   // object for MetaTable. It needs direct access to the links and
   // typelinks for use with EHashTable.
   friend class MetaTablePimpl;

public:
   // Constructors/Destructor
   MetaObject();
   MetaObject(size_t keyIndex);
   MetaObject(const char *pKey);
   MetaObject(const MetaObject &other)
      : Super(), links(), typelinks(), key(other.key),
        type(NULL), keyIdx(other.keyIdx)
   {
   }

   virtual ~MetaObject() {}

   //
   // MetaObject::setType
   //
   // This will set the MetaObject's internal type to its RTTI class name. This
   // is really only for use by MetaTable but calling it yourself wouldn't screw
   // anything up. It's just redundant.
   //
   void setType() { type = getClassName(); }

   const char *getKey()    const { return key;    }
   size_t      getKeyIdx() const { return keyIdx; }

   // Virtual Methods

   //
   // MetaObject::clone
   //
   // Virtual factory method for metaobjects; when invoked through the metatable,
   // a descendent class will return an object of the proper type matching itself.
   // This base class implementation doesn't really do anything, but I'm not fond
   // of pure virtuals so here it is. Don't call it from the parent implementation
   // as that is not what any of the implementations should do.
   //
   virtual MetaObject *clone()    const { return new MetaObject(*this); }
   virtual const char *toString() const;   
};

// MetaObject specializations for basic types

//
// MetaInteger
//
// Wrap a simple int value.
//
class MetaInteger : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaInteger, MetaObject)

protected:
   int value;

public:
   MetaInteger() : Super(), value(0) {}
   MetaInteger(size_t keyIndex, int i)
      : Super(keyIndex), value(i)
   {
   }
   MetaInteger(const char *key, int i) 
      : Super(key), value(i)
   {
   }
   MetaInteger(const MetaInteger &other)
      : Super(other), value(other.value)
   {
   }

   // Virtual Methods
   virtual MetaObject *clone()    const { return new MetaInteger(*this); }
   virtual const char *toString() const; 

   // Accessors
   int getValue() const { return value; }
   void setValue(int i) { value = i;    }

   friend class MetaTable;
};

//
// MetaDouble
//
// Wrap a simple double floating-point value.
//
class MetaDouble : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaDouble, MetaObject)

protected:
   double value;

public:
   MetaDouble() : Super(), value(0.0) {}
   MetaDouble(const char *key, double d)
      : Super(key), value(d)
   {
   }
   MetaDouble(const MetaDouble &other)
      : Super(other), value(other.value)
   {
   }

   // Virtual Methods
   virtual MetaObject *clone()    const { return new MetaDouble(*this); }
   virtual const char *toString() const;

   // Accessors
   double getValue() const { return value; }
   void setValue(double d) { value = d;    }

   friend class MetaTable;
};

//
// MetaString
//
// Wrap a dynamically allocated string value. The string is owned by the
// MetaObject.
//
class MetaString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaString, MetaObject)

protected:
   char *value;

public:
   MetaString() : Super(), value(estrdup("")) {}
   MetaString(const char *key, const char *s)
      : Super(key), value(estrdup(s))
   {
   }
   MetaString(const MetaString &other)
      : Super(other), value(estrdup(other.value))
   {
   }
   virtual ~MetaString()
   {
      if(value)
         efree(value);
      value = NULL;
   }

   // Virtual Methods
   virtual MetaObject *clone()    const { return new MetaString(*this); }
   virtual const char *toString() const { return value; }

   // Accessors
   const char *getValue() const { return value; }
   void setValue(const char *s, char **ret = NULL);

   friend class MetaTable;
};

//
// MetaConstString
//
// Wrap a string constant/literal value. The string is *not* owned by the
// MetaObject and can be shared between multiple MetaTable properties.
//
class MetaConstString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaConstString, MetaObject)

protected:
   const char *value;

public:
   MetaConstString() : Super(), value(NULL) {}
   MetaConstString(size_t keyIndex, const char *s)
      : Super(keyIndex), value(s)
   {
   }
   MetaConstString(const char *key, const char *s)
      : Super(key), value(s)
   {
   }
   MetaConstString(const MetaConstString &other)
      : Super(other), value(other.value)
   {
   }
   virtual ~MetaConstString() {}

   // Virtual Methods
   virtual MetaObject *clone()    const { return new MetaConstString(*this); }
   virtual const char *toString() const { return value; }

   // Accessors
   const char *getValue() const { return value; }
   void setValue(const char *s) { value = s;    }

   friend class MetaTable;
};

//
// MetaTable
//
// The MetaTable is a MetaObject which may itself manage other MetaObjects in
// the form of an associative array (or hash table, in other words) by key and
// by type. It is more or less exactly equivalent in functionality to an object
// in the JavaScript language.
//
class MetaTable : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaTable, MetaObject)

private:
   // Private implementation details are in metaapi.cpp
   MetaTablePimpl *pImpl;

public:
   MetaTable();
   MetaTable(const char *name);
   MetaTable(const MetaTable &other);
   virtual ~MetaTable();

   // MetaObject overrides
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // EHashTable API exposures
   float        getLoadFactor() const; // returns load factor of the key hash table
   unsigned int getNumItems()   const; // returns number of items in the table

   // Search functions. Frankly, it's more efficient to just use the "get" routines :P
   bool hasKey(const char *key);
   bool hasType(const char *type);
   bool hasKeyAndType(const char *key, const char *type);

   // Count functions.
   int countOfKey(const char *key);
   int countOfType(const char *type);
   int countOfKeyAndType(const char *key, const char *type);

   // Add/Remove Objects
   void addObject(MetaObject *object);
   void addObject(MetaObject &object);
   void removeObject(MetaObject *object);
   void removeObject(MetaObject &object);

   // Find objects in the table:
   // * By Key
   MetaObject *getObject(const char *key);
   MetaObject *getObject(size_t keyIndex);
   // * By Type
   MetaObject *getObjectType(const char *type);
   MetaObject *getObjectType(const MetaObject::Type &type);
   // * By Key AND Type
   MetaObject *getObjectKeyAndType(const char *key, const MetaObject::Type *type);
   MetaObject *getObjectKeyAndType(const char *key, const char *type);
   MetaObject *getObjectKeyAndType(size_t keyIndex, const MetaObject::Type *type);
   MetaObject *getObjectKeyAndType(size_t keyIndex, const char *type);

   // Template finders
   template<typename M> M *getObjectTypeEx()
   {
      return static_cast<M *>(getObjectType(M::StaticType));
   }

   template<typename M> M *getObjectKeyAndTypeEx(size_t keyIndex)
   {
      return static_cast<M *>(getObjectKeyAndType(keyIndex, RTTI(M)));
   }

   // Iterators
   // * By Key
   MetaObject *getNextObject(MetaObject *object, const char *key);
   MetaObject *getNextObject(MetaObject *object, size_t keyIndex);
   // * By Type
   MetaObject *getNextType(MetaObject *object, const char *type);
   MetaObject *getNextType(MetaObject *object, const MetaObject::Type *type);
   // * By Key AND Type
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, const char *type);
   MetaObject *getNextKeyAndType(MetaObject *object, size_t keyIdx,   const char *type);
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, const MetaObject::Type *type);
   MetaObject *getNextKeyAndType(MetaObject *object, size_t keyIdx,   const MetaObject::Type *type);
   // * Full table iterators
   MetaObject *tableIterator(MetaObject *object) const;
   const MetaObject *tableIterator(const MetaObject *object) const;

   // Template iterators
   template<typename M> M *getNextTypeEx(M *object)
   {
      return static_cast<M *>(getNextType(object, RTTI(M)));
   }

   template<typename M> M *getNextKeyAndTypeEx(M *object, const char *key)
   {
      return static_cast<M *>(getNextKeyAndType(object, key, RTTI(M)));
   }

   template<typename M> M *getNextKeyAndTypeEx(M *object, size_t keyIdx)
   {
      return static_cast<M *>(getNextKeyAndType(object, keyIdx, RTTI(M)));
   }

   // Add/Get/Set Convenience Methods for Basic MetaObjects
   
   // Signed integer
   void addInt(size_t keyIndex, int value);
   void addInt(const char *key, int value);
   int  getInt(size_t keyIndex, int defValue);
   int  getInt(const char *key, int defValue);
   void setInt(size_t keyIndex, int newValue);
   void setInt(const char *key, int newValue);
   int  removeInt(const char *key);

   // Double floating-point
   void   addDouble(const char *key, double value);
   double getDouble(const char *key, double defValue);
   void   setDouble(const char *key, double newValue);
   double removeDouble(const char *key);

   // Managed strings
   void        addString(const char *key, const char *value);
   const char *getString(const char *key, const char *defValue);
   void        setString(const char *key, const char *newValue);
   char       *removeString(const char *key);
   void        removeStringNR(const char *key);

   // Constant shared strings
   void        addConstString(size_t keyIndex, const char *value);
   void        addConstString(const char *key, const char *value);
   const char *getConstString(const char *key, const char *defValue);
   void        setConstString(size_t keyIndex, const char *newValue);
   void        setConstString(const char *key, const char *newValue);
   const char *removeConstString(const char *key);

   // Copy routine - clones the entire MetaTable
   void copyTableTo(MetaTable *dest) const;
   void copyTableFrom(const MetaTable *source);

   // Clearing
   void clearTable();

   // Statics
   static size_t IndexForKey(const char *key);
};

//
// MetaKeyIndex
//
// This class can be used as a static key proxy. The first time it is used, it
// will intern the provided key in the MetaObject key interning table. From
// then on, it will return the cached index.
//
class MetaKeyIndex
{
protected:
   const char *key;
   size_t keyIndex;
   bool   haveIndex;

public:
   MetaKeyIndex(const char *pKey) : key(pKey), keyIndex(0), haveIndex(false) 
   {
   }

   size_t getIndex() 
   { 
      if(!haveIndex)
      {
         keyIndex  = MetaTable::IndexForKey(key);
         haveIndex = true;
      }
      return keyIndex;
   }

   operator size_t () { return getIndex(); }
};

#endif

// EOF

