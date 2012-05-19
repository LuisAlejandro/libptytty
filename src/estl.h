#ifndef ESTL_H_
#define ESTL_H_

#include <stdlib.h>
#include <string.h>

#include "ecb.h"

template<typename T, typename U> static inline T min (T a, U b) { return a < (T)b ? a : (T)b; }
template<typename T, typename U> static inline T max (T a, U b) { return a > (T)b ? a : (T)b; }

template<typename T, typename U> static inline void swap (T& a, U& b) { T t = a; a = (T)b; b = (U)t; }

template <typename I, typename T>
I find (I first, I last, const T& value)
{
  while (first != last && *first != value)
    ++first;

  return first;
}

#include <new>

#if __cplusplus >= 201103L
  #include <type_traits>
#endif

// original version taken from MICO, but this has been completely rewritten
// known limitations w.r.t. std::vector
// - many methods missing
// - no error checking, no exceptions thrown
// - size_type is 32bit even on 64 bit hosts, so limited to 2**31 elements
// - no allocator support
// - we don't really care about const correctness, but we try
// - we don't care about namespaces and stupid macros the user might define
template<class T>
struct simplevec
{
#if ESTL_BIG_VECTOR
  // shoudl use size_t/ssize_t, but that's not portable enough for us
  typedef unsigned long size_type;
  typedef          long difference_type;
#else
  typedef uint32_t size_type;
  typedef  int32_t difference_type;
#endif

  typedef       T  value_type;
  typedef       T *iterator;
  typedef const T *const_iterator;
  typedef       T *pointer;
  typedef const T *const_pointer;
  typedef       T &reference;
  typedef const T &const_reference;
  // missing: allocator_type
  // missing: reverse iterator

private:
  size_type sze, res;
  T *buf;

  // we shamelessly optimise for "simple" types. everything
  // "not simple enough" will use the slow path.
  static bool is_simple_enough ()
  {
    return 1; // we are not there yet
    #if __cplusplus >= 201103L
      return std::is_trivially_assignable<T, T>::value
          && std::is_trivially_constructable<T>::value
          && std::is_trivially_copyable<T>::value
          && std::is_trivially_destructible<T>::value;
    #elif ECB_GCC_VERSION(4,4)
      return __has_trivial_assign (T)
          && __has_trivial_constructor (T)
          && __has_trivial_copy (T)
          && __has_trivial_destructor (T);
    #else
      return 0;
    #endif
  }

  static void construct (iterator a, size_type n = 1)
  {
    if (!is_simple_enough ())
      while (n--)
        new (*a++) T ();
  }

  static void destruct (iterator a, size_type n = 1)
  {
    if (!is_simple_enough ())
      while (n--)
        (*a++).~T ();
  }

  static void cop_new (iterator a, iterator b) { new (a) T (*b); }
  static void cop_set (iterator a, iterator b) {     *a  =  *b ; }

  // these copy helpers actually use the copy constructor, not assignment
  static void copy_lower (iterator dst, iterator src, size_type n, void (*op)(iterator, iterator) = cop_new)
  {
    if (is_simple_enough ())
      memmove (dst, src, sizeof (T) * n);
    else
      while (n--)
        op (dst++, src++);
  }

  static void copy_higher (iterator dst, iterator src, size_type n, void (*op)(iterator, iterator) = cop_new)
  {
    if (is_simple_enough ())
      memmove (dst, src, sizeof (T) * n);
    else
      while (n--)
        op (dst + n, src + n);
  }

  static void copy (iterator dst, iterator src, size_type n, void (*op)(iterator, iterator) = cop_new)
  {
    if (is_simple_enough ())
      memcpy (dst, src, sizeof (T) * n);
    else
      copy_lower (dst, src, n, op);
  }

  static T *alloc (size_type n) ecb_cold
  {
    return (T *)::operator new ((size_t) (sizeof (T) * n));
  }

  void dealloc () ecb_cold
  {
    destruct (buf, sze);
    ::operator delete (buf);
  }

  size_type good_size (size_type n) ecb_cold
  {
    return n ? 2UL << ecb_ld32 (n) : 5;
  }

  void ins (iterator where, size_type n)
  {
    size_type pos = where - begin ();

    if (ecb_expect_false (sze + n > res))
      {
        res = good_size (sze + n);

        T *nbuf = alloc (res);
        copy (nbuf, buf, sze, cop_new);
        dealloc ();
        buf = nbuf;
      }

    construct (buf + sze, n);
    copy_higher (buf + pos + n, buf + pos, sze - pos, cop_set);
    sze += n;
  }

public:
  size_type capacity () const { return res; }
  size_type size     () const { return sze; }
  bool empty         () const { return size () == 0; }

  const_iterator  begin () const { return &buf [      0]; }
        iterator  begin ()       { return &buf [      0]; }
  const_iterator  end   () const { return &buf [sze    ]; }
        iterator  end   ()       { return &buf [sze    ]; }
  const_reference front () const { return  buf [      0]; }
        reference front ()       { return  buf [      0]; }
  const_reference back  () const { return  buf [sze - 1]; }
        reference back  ()       { return  buf [sze - 1]; }

  void reserve (size_type sz)
  {
    if (ecb_expect_true (sz <= res))
      return;

    sz = good_size (sz);
    T *nbuf = alloc (sz);

    copy (nbuf, begin (), sze);
    dealloc ();

    buf  = nbuf;
    res = sz;
  }

  void resize (size_type sz)
  {
    reserve (sz);

    if (is_simple_enough ())
      sze = sz;
    else
      {
        while (sze < sz) construct (buf + sze++);
        while (sze > sz) destruct  (buf + --sze);
      }
  }

  simplevec ()
  : sze(0), res(0), buf(0)
  {
  }

  simplevec (size_type n, const T &t = T ())
  : sze(0), res(0), buf(0)
  {
    insert (begin (), n, t);
  }

  simplevec (const_iterator first, const_iterator last)
  : sze(0), res(0), buf(0)
  {
    insert (begin (), first, last);
  }

  simplevec (const simplevec<T> &v)
  : sze(0), res(0), buf(0)
  {
    insert (begin (), v.begin (), v.end ());
  }

  simplevec<T> &operator= (const simplevec<T> &v)
  {
    swap (simplevec<T> (v));
    return *this;
  }

  ~simplevec ()
  {
    dealloc ();
  }

  void swap (simplevec<T> &t)
  {
    ::swap (sze, t.sze);
    ::swap (res, t.res);
    ::swap (buf, t.buf);
  }

  void clear ()
  {
    destruct (buf, sze);
    sze = 0;
  }

  void push_back (const T &t)
  {
    reserve (sze + 1);
    new (buf + sze++) T (t);
  }

  void pop_back ()
  {
    destruct (buf + --sze);
  }

  const T &operator [](size_type idx) const { return buf[idx]; }
        T &operator [](size_type idx)       { return buf[idx]; }

  iterator insert (iterator pos, const T &t)
  {
    size_type at = pos - begin ();
    ins (pos, 1);
    buf [pos] = t;
    return pos;
  }

  iterator insert (iterator pos, const_iterator first, const_iterator last)
  {
    size_type n  = last - first;
    size_type at = pos - begin ();

    ins (pos, n);
    copy (pos, first, n, cop_set);

    return pos;
  }

  iterator insert (iterator pos, size_type n, const T &t)
  {
    size_type at = pos - begin ();

    ins (pos, n);

    for (size_type i = 0; i < n; ++i)
      buf [at + i] = t;

    return pos;
  }

  void erase (iterator first, iterator last)
  {
    size_t n = last - first;

    copy_lower (last, first, end () - last, cop_set);
    sze -= n;
    destruct (buf + sze, n);
  }

  void erase (iterator pos)
  {
    if (pos != end ())
      erase (pos, pos + 1);
  }
};

template<class T>
bool operator ==(const simplevec<T> &v1, const simplevec<T> &v2)
{
  if (v1.size () != v2.size ()) return false;

  return !v1.size () || !memcmp (&v1[0], &v2[0], v1.size () * sizeof (T));
}

template<class T>
bool operator <(const simplevec<T> &v1, const simplevec<T> &v2)
{
  unsigned long minlast = min (v1.size (), v2.size ());

  for (unsigned long i = 0; i < minlast; ++i)
    {
      if (v1[i] < v2[i]) return true;
      if (v2[i] < v1[i]) return false;
    }
  return v1.size () < v2.size ();
}

template<typename T>
struct vector : simplevec<T>
{
};

#endif

