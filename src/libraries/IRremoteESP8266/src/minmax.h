#ifndef __MINMAX_H__
#define __MINMAX_H__

template<typename A,typename B> A min(A a,B b)
{
  return a<b?a:b;
}

template<typename A,typename B> A max(A a,B b)
{
  return a>b?a:b;
}


#endif //__MINMAX_H__