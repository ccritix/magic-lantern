/*
 * ANSI C Library for maintainance of AVL Balanced Trees
 *
 * ref.:
 *  G. M. Adelson-Velskij & E. M. Landis
 *  Doklady Akad. Nauk SSSR 146 (1962), 263-266
 *
 * see also:
 *  D. E. Knuth: The Art of Computer Programming Vol.3 (Sorting and Searching)
 *
 * (C) 2000 Daniel Nagy, Budapest University of Technology and Economics
 * Released under GNU General Public License (GPL) version 2
 *
 */

#include "avl.h"

/* Private methods */

/* Swing to the left
 * Warning: no balance maintainance
 */
static void avl_swl(avl** root){
   avl* a=*root;
   avl* b=a->right;
   *root=b;
   a->right=b->left;
   b->left=a;
}

/* Swing to the right
 * Warning: no balance maintainance
 */
static void avl_swr(avl** root){
   avl* a=*root;
   avl* b=a->left;
   *root=b;
   a->left=b->right;
   b->right=a;
}

/* Balance maintainance after especially nasty swings
 */
static void avl_nasty(avl* root){
   switch(root->balance){
    case -1:root->left->balance=0;
      root->right->balance=1;
      break;
    case 1:	root->left->balance=-1;
      root->right->balance=0;
      break;
    case 0:	root->left->balance=0;
      root->right->balance=0;
   }
   root->balance=0;
}

/* Public methods */

/* Insert element a into the AVL tree t
 * returns 1 if the depth of the tree has grown
 * Warning: do not insert elements already present
 */
static int avl_insert(avl_tree* t,avl* a)
{
   /* initialize */
   a->left=0;
   a->right=0;
   a->balance=0;
   /* insert into an empty tree */
   if(!t->root){
      t->root=a;
      return 1;
   }
   
   if(t->compar(t->root,a)>0){
      /* insert into the left subtree */
      if(t->root->left){
	 avl_tree left_subtree;
	 left_subtree.root=t->root->left;
	 left_subtree.compar=t->compar;
	 if(avl_insert(&left_subtree,a)){
	    switch(t->root->balance--){
	     case 1: return 0;
	     case 0:	return 1;
	    }
	    if(t->root->left->balance<0){
	       avl_swr(&(t->root));
	       t->root->balance=0;
	       t->root->right->balance=0;
	    }else{
	       avl_swl(&(t->root->left));
	       avl_swr(&(t->root));
	       avl_nasty(t->root);
	    }
	 }else t->root->left=left_subtree.root;
	 return 0;
      }else{
	 t->root->left=a;
	 if(t->root->balance--) return 0;
	 return 1;
      }
   }else{
      /* insert into the right subtree */
      if(t->root->right){
	 avl_tree right_subtree;
	 right_subtree.root=t->root->right;
	 right_subtree.compar=t->compar;
	 if(avl_insert(&right_subtree,a)){
	    switch(t->root->balance++){
	     case -1: return 0;
	     case 0: return 1;
	    }
	    if(t->root->right->balance>0){
	       avl_swl(&(t->root));
	       t->root->balance=0;
	       t->root->left->balance=0;
	    }else{
	       avl_swr(&(t->root->right));
	       avl_swl(&(t->root));
	       avl_nasty(t->root);
	    }
	 }else t->root->right=right_subtree.root;
	 return 0;
      }else{
	 t->root->right=a;
	 if(t->root->balance++) return 0;
	 return 1;
      }
   }
}

/* Remove an element a from the AVL tree t
 * returns -1 if the depth of the tree has shrunk
 * Warning: if the element is not present in the tree,
 *          returns 0 as if it had been removed succesfully.
 */
static int avl_remove(avl_tree* t, avl* a)
{
   int b;
   if(t->root==a)
     return avl_removeroot(t);
   b=t->compar(t->root,a);
   if(b>=0){
      /* remove from the left subtree */
      int ch;
      avl_tree left_subtree;
      if((left_subtree.root=t->root->left)){
	 left_subtree.compar=t->compar;
	 ch=avl_remove(&left_subtree,a);
	 t->root->left=left_subtree.root;
	 if(ch){
	    switch(t->root->balance++){
	     case -1: return -1;
	     case 0: return 0;
	    }
	    switch(t->root->right->balance){
	     case 0:	avl_swl(&(t->root));
	       t->root->balance=-1;
	       t->root->left->balance=1;
	       return 0;
	     case 1: avl_swl(&(t->root));
	       t->root->balance=0;
	       t->root->left->balance=0;
	       return -1;
	    }
	    avl_swr(&(t->root->right));
	    avl_swl(&(t->root));
	    avl_nasty(t->root);
	    return -1;
	 }
      }
   }
   if(b<=0){
      /* remove from the right subtree */
      int ch;
      avl_tree right_subtree;
      if((right_subtree.root=t->root->right)){
	 right_subtree.compar=t->compar;
	 ch=avl_remove(&right_subtree,a);
	 t->root->right=right_subtree.root;
	 if(ch){
	    switch(t->root->balance--){
	     case 1: return -1;
	     case 0: return 0;
	    }
	    switch(t->root->left->balance){
	     case 0:	avl_swr(&(t->root));
	       t->root->balance=1;
	       t->root->right->balance=-1;
	       return 0;
	     case -1:avl_swr(&(t->root));
	       t->root->balance=0;
	       t->root->right->balance=0;
	       return -1;
	    }
	    avl_swl(&(t->root->left));
	    avl_swr(&(t->root));
	    avl_nasty(t->root);
	    return -1;
	 }
      }
   }
   return 0;
}

/* Remove the root of the AVL tree t
 * Warning: dumps core if t is empty
 */
static int avl_removeroot(avl_tree* t)
{
   int ch;
   avl* a;
   if(!t->root->left){
      if(!t->root->right){
	 t->root=0;
	 return -1;
      }
      t->root=t->root->right;
      return -1;
   }
   if(!t->root->right){
      t->root=t->root->left;
      return -1;
   }
   if(t->root->balance<0){
      /* remove from the left subtree */
      a=t->root->left;
      while(a->right) a=a->right;
   }else{
      /* remove from the right subtree */
      a=t->root->right;
      while(a->left) a=a->left;
   }
   ch=avl_remove(t,a);
   a->left=t->root->left;
   a->right=t->root->right;
   a->balance=t->root->balance;
   t->root=a;
   if(a->balance==0) return ch;
   return 0;
}

/* Iterate through elements in t from a range between a and b (inclusive)
 * for each element calls iter(a) until it returns 0
 * returns the last value returned by iterator or 0 if there were no calls
 * Warning: a<=b must hold
 */
static int avl_range(avl_tree* t,avl* a,avl* b,int(*iter)(avl* a))
{
   int x,c=0;
   if(!t->root) return 0;
   x=t->compar(t->root,a);
   if(a!=b){
      if(x<0){
	 x=t->compar(t->root,b);
	 if(x>0) x=0;
      }
   }
   if(x>=0){
      /* search in the left subtree */
      avl_tree left_subtree;
      if((left_subtree.root=t->root->left)){
	 left_subtree.compar=t->compar;
	 if(!(c=avl_range(&left_subtree,a,b,iter))) if(x>0) return 0;
      }
   }
   if(x==0){
      if(!(c=iter(t->root))) return 0;
   }
   if(x<=0){
      /* search in the right subtree */
      avl_tree right_subtree;
      if((right_subtree.root=t->root->right)){
	 right_subtree.compar=t->compar;
	 if(!(c=avl_range(&right_subtree,a,b,iter))) if(x<0) return 0;
      }
   }
   return c;
}

/* Iterate through elements in t equal to a
 * for each element calls iter(a) until it returns 0
 * returns the last value returned by iterator or 0 if there were no calls
 */
static int avl_search(avl_tree* t,avl* a,int(*iter)(avl* a))
{
   return avl_range(t,a,a,iter);
}

/* consistency check (0=OK) */
static int checktree(avl* a, int(*compar)(void* a,void* b)){
    int err = 0;
    if(a==0) return 0;
    if(a->right)
    {
        if (compar(a, a->right) >= 0) err |= 1;
        err |= checktree(a->right, compar);
    }
    if(a->left)
    {
        if (compar(a, a->left) <= 0) err |= 2;
        err |= checktree(a->left, compar);
    }
    return err;
}
