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
//    Generic hash table implementation.
//
//-----------------------------------------------------------------------------

#ifndef E_HASH_H__
#define E_HASH_H__

#include "e_hashkeys.h"
#include "m_dllist.h"

//
// EHashTable<item_type, key_type, hashKey, linkPtr>
//
// This class replaces the ehash_t structure to provide a generic,
// high-performance hash table which utilizes the in-structure link capability
// provided by DLListItem<T>, meaning no cache-harmful separately allocated
// links to traverse through.
//
// Provide the template the primary type of object to be stored in item_type, 
// and the type of HashKey class being used in key_type (see e_hashkeys.h for
// some basic key class types that implement the required interface).
//
// Pass the pointer-to-member from the contained type for the key and the
// link (must be a DLListItem<T>). The hash table uses these pointer-to-
// members on all the objects in the hash so that it can manipulate the
// objects without knowing anything else about their type or layout.
//
template<typename item_type, typename key_type, 
         typename key_type::basic_type const item_type::* hashKey, 
         DLListItem<item_type> item_type::* linkPtr>
class EHashTable
{
public:
   typedef DLListItem<item_type> link_type;    // Type of linked list item
   
   // Type of key's basic data member
   typedef typename key_type::basic_type basic_key_type; 
   typedef typename key_type::param_type param_key_type;

protected:
   link_type    **chains;      // hash chains
   bool           isInit;      // true if hash is initialized
   unsigned int   numChains;   // number of chains
   unsigned int   numItems;    // number of items currently in table
   float          loadFactor;  // load = numitems / numchains
   int            iteratorPos; // current position of tableIterator
   
   void calcLoadFactor() { loadFactor = (float)numItems / numChains; }

public:
   //
   // Constructor
   //
   EHashTable() 
      : chains(NULL), isInit(false), numChains(0), numItems(0), 
        loadFactor(0.0f), iteratorPos(-1)
   {
   }

   EHashTable(unsigned int pNumChains)
      : chains(NULL), isInit(false), numChains(0), numItems(0),
        loadFactor(0.0f), iteratorPos(-1)
   {
      initialize(pNumChains);
   }

   // Basic accessors
   int   isInitialized() const { return isInit;     }
   float getLoadFactor() const { return loadFactor; }
   
   unsigned int getNumItems()  const { return numItems;  }
   unsigned int getNumChains() const { return numChains; }

   // 
   // initialize
   //
   // Initializes a hash table. This is currently done here rather than in the
   // constructor primarily because EHashTables are generally global objects,
   // and therefore they cannot safely call Z_Malloc during C++ init. I'd rather
   // this move to the constructor eventually.
   //
   void initialize(unsigned int pNumChains)
   {
      if(!isInit)
      {
         numChains = pNumChains;
         chains    = ecalloc(link_type **, numChains, sizeof(link_type *));
         isInit    = true;
      }
   }

   //
   // destroy
   //
   // Frees the hash table's chains. Again this would be better in the 
   // destructor, but it should remain here until/unless initialization is moved
   // into the constructor, so it remains balanced.
   //
   void destroy()
   {
      if(chains)
         efree(chains);

      chains      =  NULL;
      isInit      =  false;
      numChains   =  0;
      numItems    =  0;
      loadFactor  =  0.0f;
      iteratorPos = -1;
   }

   //
   // addObject
   //
   // Put an object into the hash table.
   // Overload taking a pre-computed unmodulated hash code.
   //
   void addObject(item_type &object, unsigned int unmodHC)
   {
      if(!isInit)
         initialize(127);

      link_type &link = object.*linkPtr;
      link.dllData    = unmodHC; 
      unsigned int hc = link.dllData % numChains;

      link.insert(&object, &chains[hc]);

      ++numItems;
      calcLoadFactor();
   }

   void addObject(item_type &object)
   {
      addObject(object, key_type::HashCode(object.*hashKey));
   }

   // Convenience overloads for pointers
   void addObject(item_type *object) { addObject(*object); }
   void addObject(item_type *object, unsigned int unmodHC)
   {
      addObject(*object, unmodHC);
   }

   //
   // removeObject
   //
   // Removes an object from the hash table, provided it is in the
   // hash table already.
   //
   void removeObject(item_type &object)
   {
      if(isInit)
      {
         link_type &link = object.*linkPtr;
         link.remove();

         --numItems;
         calcLoadFactor();
      }
   }

   // Convenience overload for pointers
   void removeObject(item_type *object) { removeObject(*object); }

   //
   // objectForKey(EHashUnmodKey, unsigned int)
   //
   // Tries to find an object, given an unmodulated pre-computed 
   // hash code corresponding to the key.
   //
   item_type *objectForKey(param_key_type key, unsigned int unmodHC) const
   {
      if(isInit)
      {
         unsigned int hc = unmodHC % numChains;
         link_type *chain = chains[hc];

         while(chain && !key_type::Compare(chain->dllObject->*hashKey, key))
            chain = chain->dllNext;

         return chain ? chain->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // objectForKey(key_type&)
   //
   // Tries to find an object for the given key in the hash table. 
   // Takes an argument of the key object type's basic_type typedef. 
   // ie., an int, const char *, etc.
   //
   item_type *objectForKey(param_key_type key) const
   {
      return objectForKey(key, key_type::HashCode(key));
   }

   //
   // chainForKey(EHashUnmodKey, unsigned int)
   //
   // Returns the first object on the hash chain used by the given
   // unmodulated hash code, or NULL if that hash chain is empty. The
   // object returned does not necessarily match the given hash code.
   //
   item_type *chainForKey(param_key_type key, unsigned int unmodHC)
   {
      if(isInit)
      {
         unsigned int hc  = unmodHC % numChains;
         link_type *chain = chains[hc];

         return chain ? chain->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // chainForKey(key_type&)
   //
   // Returns the first object on the hash chain used by the given key, or NULL
   // if that hash chain is empty. The object returned does not necessarily 
   // match the given key.
   //
   item_type *chainForKey(param_key_type key) const
   {
      return chainForKey(key, key_type::HashCode(key));
   }

   //
   // nextOnChain
   //
   // Returns the next object on the same hash chain, which may or may not
   // have the same key as the previous object.
   //
   item_type *nextOnChain(item_type *object) const
   {
      if(isInit)
      {
         link_type &link = object->*linkPtr;

         return link.dllNext ? link.dllNext->dllObject : NULL;
      }
      else
         return NULL;
   }

   //
   // keyForObject
   //
   // Retrieves a key from an object in the hash table.
   //
   basic_key_type keyForObject(item_type *object) const
   {
      return object->*hashKey;
   }

   // 
   // keyIterator
   //
   // Looks for the next object after the current one specified having the
   // same key. If passed NULL in object, it will start a new search.
   // Returns NULL when the search has reached the end of the hash chain.
   // Overload for pre-computed unmodulated hash codes.
   //
   item_type *keyIterator(item_type *object, param_key_type key,
                          unsigned int unmodHC)
   {
      item_type *ret;

      if(!isInit)
         return NULL;

      if(!object) // starting a new search?
         ret = objectForKey(key, unmodHC);
      else
      {
         link_type *link = &(object->*linkPtr);

         // start on next object in hash chain
         link = link->dllNext;

         // walk down the chain
         while(link && !key_type::Compare(link->dllObject->*hashKey, key))
            link = link->dllNext;

         ret = link ? link->dllObject : NULL;
      }

      return ret;
   }

   //
   // keyIterator
   //
   // Looks for the next object after the current one specified having the
   // same key. If passed NULL in object, it will start a new search.
   // Returns NULL when the search has reached the end of the hash chain.
   //
   item_type *keyIterator(item_type *object, param_key_type key)
   {
      item_type *ret;

      if(!isInit)
         return NULL;

      if(!object) // starting a new search?
         ret = objectForKey(key);
      else
      {
         link_type *link = &(object->*linkPtr);

         // start on next object in hash chain
         link = link->dllNext;

         // walk down the chain
         while(link && !key_type::Compare(link->dllObject->*hashKey, key))
            link = link->dllNext;

         ret = link ? link->dllObject : NULL;
      }

      return ret;
   }

   //
   // tableIterator
   //
   // Iterates over all objects in a hash table, in chain order.
   // Pass NULL in object to start a new search. NULL is returned when the 
   // entire table has been iterated over.
   //
   const item_type *tableIterator(const item_type *object)
   {
      const item_type *ret = NULL;

      if(!isInit)
         return NULL;

      // already searching?
      if(object)
      {
         // is there another object on the same chain?
         const link_type &link = object->*linkPtr;

         if(link.dllNext)
            ret = link.dllNext->dllObject;
      }
      else
         iteratorPos = -1; // starting a new search, reset position

      // search for the next chain with an object in it
      while(!ret && ++iteratorPos < (signed int)numChains)
      {
         link_type *chain = chains[iteratorPos];

         if(chain)
            ret = chain->dllObject;
      }

      return ret;
   }

   //
   // tableIterator
   //
   // Mutable pointer overload.
   //
   item_type *tableIterator(item_type *object)
   {
      return const_cast<item_type *>(tableIterator(const_cast<const item_type *>(object)));
   }

   //
   // rebuild
   //
   // Rehashes all objects in the table, in the event that the load factor has
   // exceeded acceptable levels. Rehashing policy is determined by user code.
   //
   void rebuild(unsigned int newNumChains)
   {
      link_type    **oldchains    = chains;    // save current chains
      link_type    **prevobjs     = NULL;
      unsigned int   oldNumChains = numChains;
      unsigned int   i;

      if(!isInit)
         return;

      // allocate a new chains table
      chains = ecalloc(link_type **, newNumChains, sizeof(link_type *));
      numChains = newNumChains;

      // allocate a temporary table of list end pointers, for each new chain
      prevobjs = ecalloc(link_type **, newNumChains, sizeof(link_type *));

      // run down the old chains
      for(i = 0; i < oldNumChains; ++i)
      {
         link_type *chain;
         unsigned int hashcode;

         // restart from beginning of old chain each time
         while((chain = oldchains[i]))
         {
            // remove from old hash
            chain->remove();

            // re-add to new hash at end of list
            hashcode = chain->dllData % numChains;

            chain->insert(chain->dllObject, 
                          prevobjs[hashcode] ? &(prevobjs[hashcode]->dllNext) :
                                               &(chains[hashcode]));

            prevobjs[hashcode] = chain;
         }
      }

      // recalculate load factor
      calcLoadFactor();

      // delete old chains
      efree(oldchains);

      // delete temporary list end pointers
      efree(prevobjs);
   }
};

#endif

// EOF

