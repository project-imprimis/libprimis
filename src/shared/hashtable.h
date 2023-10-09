#ifndef HASHTABLE_H_
#define HASHTABLE_H_

inline uint hthash(const char *key)
{
    uint h = 5381;
    for(int i = 0, k; (k = key[i]); i++)
    {
        h = ((h<<5)+h)^k;    // bernstein k=33 xor
    }
    return h;
}

inline bool htcmp(const char *x, const char *y)
{
    return !strcmp(x, y);
}

inline uint hthash(int key)
{
    return key;
}

inline bool htcmp(int x, int y)
{
    return x==y;
}

inline uint hthash(GLuint key)
{
    return key;
}

inline bool htcmp(GLuint x, GLuint y)
{
    return x==y;
}

/**
 * A specializable hash base class for the creation of hash tables and other objects.
 *
 * Implemented via the hash chaining method; there are DEFAULTSIZE chains that can
 * have multiple elements in the case of a hash collision, connected by a single
 * directional linked list. The base size of the hash table is fixed at the
 * construction of the hash table and cannot be changed later.
 *
 * Note that E (the element type being stored) and T (the type that gets added) are
 * not necessarily the same.
 *
 * @tparam H The inheriting object
 * @tparam E The type of the element type to be stored
 * @tparam K the type of the element's key
 * @tparam T the type of the data being added
 */
template<class H, class E, class K, class T>
struct hashbase
{
    typedef E elemtype;
    typedef K keytype;
    typedef T datatype;

    enum { CHUNKSIZE = 64 };

    /**
     * @brief A chain entry. Capable of being created into a singly linked list.
     */
    struct chain { E elem; chain *next; };

    /**
     * @brief A series of chains capable of being created into a singly linked list.
     */
    struct chainchunk { chain chains[CHUNKSIZE]; chainchunk *next; };

    int size; /**< The size of the base chain array. Cannot be changed after creation. */
    int numelems; /**< Total number of elements. */
    chain **chains; /**< Pointer to the array containing the base chains.*/

    chainchunk *chunks;
    chain *unused;

    enum { DEFAULTSIZE = 1<<10 }; //2^10 = 1024*(8) = 8192 bytes before allocation

    hashbase(int size = DEFAULTSIZE)
      : size(size)
    {
        numelems = 0;  //no elements assigned by default
        chunks = nullptr; //no chunks assigned
        unused = nullptr;
        chains = new chain *[size]; //the base array of chains: an array of 1024 pointers at default size
        memset(chains, 0, size*sizeof(chain *)); //clear all values inside the array to 0
    }

    ~hashbase()
    {
        delete[] chains; //free the 1024 entry (by default) base array
        chains = nullptr;
        deletechunks();
    }

    #define HTFIND(success, fail) \
        uint h = hthash(key)&(this->size-1); \
        for(chain *c = this->chains[h]; c; c = c->next) \
        { \
            if(htcmp(key, H::getkey(c->elem))) \
            { \
                return success H::getdata(c->elem); \
            } \
        } \
        return (fail);

    /**
     * @brief Attempts to find the given element in the hash table.
     *
     * This is similar to `unordered_map::find()` except that `nullptr`, not `end()`,
     * is returned if the key is not found, and `hthash` instead of `std::hash<>` must
     * be defined.
     *
     * @param key the key to search for
     * @return a pointer to the found item, or nullptr
     */
    template<class U>
    T *access(const U &key)
    {
        HTFIND(&, nullptr);
    }

    /**
     * @brief Attempts to find the given element in the hash table.
     *
     * This is similar to `operator[]` except that `hthash()` instead of `std::hash<>`
     * must be defined for the specified element. If the element is not found, a new
     * element with the key `key` and the contents `elem` are inserted instead.
     *
     * @param key the key to search for
     * @param element the contents to conditionally add
     * @return a pointer to the found item, or to an empty element
     */
    template<class U, class V>
    T &access(const U &key, const V &elem)
    {
        HTFIND( , insert(h, key) = elem);
    }

    /**
     * @brief Attempts to find the given element in the hash table.
     *
     * This is similar to `operator[]` except that `hthash()` instead of `std::hash<>`
     * must be defined for the specified element.
     *
     * @param key the key to search for
     * @return a pointer to the found item, or to an empty element
     */
    template<class U>
    T &operator[](const U &key)
    {
        HTFIND( , insert(h, key));
    }

    /**
     * @brief Attempts to find the given element in the hash table.
     * Returns the element by reference, not by pointer. Since a reference cannot
     * be nullptr, returns the given `notfound` value if not found.
     * @param key the key to search for
     * @param notfound the value to return if not found
     * @return the element, or the value passed as `notfound`
     */
    template<class U>
    T &find(const U &key, T &notfound)
    {
        HTFIND( , notfound);
    }
    /**
     * @brief Attempts to find the given element in the hash table.
     * Returns the element by constant reference, not by pointer. Since a reference
     * cannot be nullptr, returns the given `notfound` value if not found.
     * @param key the key to search for
     * @param notfound the value to return if not found
     * @return the element, or the value passed as `notfound`
     */
    template<class U>
    const T &find(const U &key, const T &notfound)
    {
        HTFIND( , notfound);
    }

    /**
     * @brief Removes the key-element entry corresponding to the passed key.
     *
     * @param key the key of the element to erase
     * @return true if successful, false if not found
     */
    template<class U>
    bool remove(const U &key)
    {
        uint h = hthash(key)&(size-1);
        for(chain **p = &chains[h], *c = chains[h]; c; p = &c->next, c = c->next)
        {
            if(htcmp(key, H::getkey(c->elem)))
            {
                *p = c->next;
                c->elem.~E();
                new (&c->elem) E;
                c->next = unused;
                unused = c;
                numelems--;
                return true;
            }
        }
        return false;
    }

    void recycle()
    {
        if(!numelems)
        {
            return;
        }
        for(int i = 0; i < static_cast<int>(size); ++i)
        {
            chain *c = chains[i];
            if(!c)
            {
                continue;
            }
            for(;;)
            {
                htrecycle(c->elem);
                if(!c->next)
                {
                    break;
                }
                c = c->next;
            }
            c->next = unused;
            unused = chains[i];
            chains[i] = nullptr;
        }
        numelems = 0;
    }

    void deletechunks()
    {
        for(chainchunk *nextchunk; chunks; chunks = nextchunk)
        {
            nextchunk = chunks->next;
            delete chunks;
        }
    }

    void clear()
    {
        if(!numelems)
        {
            return;
        }
        memset(chains, 0, size*sizeof(chain *));
        numelems = 0;
        unused = nullptr;
        deletechunks();
    }

    static inline chain *enumnext(void *i) { return ((chain *)i)->next; }
    static inline K &enumkey(void *i) { return H::getkey(((chain *)i)->elem); }
    static inline T &enumdata(void *i) { return H::getdata(((chain *)i)->elem); }
    private:
        /**
         * @brief Creates a new hash entry using the given hash value.
         *
         * @param h The hash chain to use. Must be between 0 and `size`.
         *
         * @return A pointer to the created hash location.
         */
        chain *insert(uint h)
        {

            if(!unused)
            {
                chainchunk *chunk = new chainchunk;
                chunk->next = chunks;
                chunks = chunk;
                for(int i = 0; i < static_cast<int>(CHUNKSIZE-1); ++i)
                {
                    chunk->chains[i].next = &chunk->chains[i+1];
                }
                chunk->chains[CHUNKSIZE-1].next = unused;
                unused = chunk->chains;
            }
            chain *c = unused;
            unused = unused->next;
            c->next = chains[h];
            chains[h] = c;
            numelems++;
            return c;
        }

        template<class U>
        T &insert(uint h, const U &key)
        {
            chain *c = insert(h);
            H::setkey(c->elem, key);
            return H::getdata(c->elem);
        }
};

template<class T>
inline void htrecycle(const T &) {}

template<class T>
struct hashset : hashbase<hashset<T>, T, T, T>
{
    typedef hashbase<hashset<T>, T, T, T> basetype;

    hashset(int size = basetype::DEFAULTSIZE) : basetype(size) {}

    static inline const T &getkey(const T &elem) { return elem; }
    static inline T &getdata(T &elem) { return elem; }
    template<class K>
    static inline void setkey(T &, const K &) {}

    template<class V>
    T &add(const V &elem)
    {
        return basetype::access(elem, elem);
    }
};

template<class K, class T>
struct hashtableentry
{
    K key;
    T data;
};

template<class K, class T>
inline void htrecycle(hashtableentry<K, T> &entry)
{
    htrecycle(entry.key);
    htrecycle(entry.data);
}

template<class K, class T>
struct hashtable : hashbase<hashtable<K, T>, hashtableentry<K, T>, K, T>
{
    typedef hashbase<hashtable<K, T>, hashtableentry<K, T>, K, T> basetype;
    typedef typename basetype::elemtype elemtype;

    hashtable(int size = basetype::DEFAULTSIZE) : basetype(size) {}

    static inline K &getkey(elemtype &elem) { return elem.key; }
    static inline T &getdata(elemtype &elem) { return elem.data; }

    template<class U>
    static inline void setkey(elemtype &elem, const U &key) { elem.key = key; }
};

//ht stands for a hash table
#define ENUMERATE_KT(ht,k,e,t,f,b) \
    for(int i = 0; i < static_cast<int>((ht).size); ++i) \
    { \
        for(void *ec = (ht).chains[i]; ec;) \
        { \
            k &e = (ht).enumkey(ec); \
            t &f = (ht).enumdata(ec); \
            ec = (ht).enumnext(ec); \
            b; \
        } \
    }
#define ENUMERATE(ht,t,e,b) \
    for(int i = 0; i < static_cast<int>((ht).size); ++i) \
    { \
        for(void *ec = (ht).chains[i]; ec;) \
        { \
            t &e = (ht).enumdata(ec); \
            ec = (ht).enumnext(ec); \
            b; \
        } \
    }
#endif
