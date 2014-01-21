#ifndef _CHAIN_H
#define _CHAIN_H

/**
 * Adds element @a El at the beginning of the chain @a Head.
 */
#define chain_add(Head, El) \
do {                        \
  (El)->prev = NULL;        \
  if (Head) {               \
    (Head)->prev = El;      \
    (El)->next = Head;      \
  } else {                  \
    (El)->next = NULL;      \
  }                         \
  Head = El;                \
} while (0)

/**
 * Adds element @a El at the end of the chain @a Head.
 */
#define chain_add_tail(Head, El)    \
do {                                \
  (El)->next = NULL;                \
  if (Head) {                       \
    typeof (El) __Tail = Head;      \
    while (__Tail->next) {          \
      __Tail = __Tail->next;        \
    }                               \
    __Tail->next = El;              \
    (El)->prev = __Tail;            \
  } else {                          \
    (El)->prev = NULL;              \
    Head = El;                      \
  }                                 \
} while (0)

/**
 * Inserts element @a El in the chain @a Head before element @a Before.
 */
#define chain_insert(Head, Before, El)  \
do {                                    \
  if (Before == Head) {                 \
    (El)->prev = NULL;                  \
    (El)->next = Before;                \
    (Before)->prev = El;                \
    Head = El;                          \
  } else {                              \
    (El)->prev = (Before)->prev;        \
    (El)->prev->next = El;              \
    (El)->next = Before;                \
    (Before)->prev = El;                \
  }                                     \
} while (0)

/**
 * Removes element @a El from the chain @a Head.
 */
#define chain_del(Head, El)         \
do {                                \
  typeof (El) prev = (El)->prev;    \
  typeof (El) next = (El)->next;    \
  if (prev) {                       \
    prev->next = next;              \
  } else {                          \
    Head = next;                    \
  }                                 \
  if (next) {                       \
    next->prev = prev;              \
  }                                 \
} while (0)

/**
 * Simple iteration over elements of the chain.
 */
#define chain_for_each(Head, El) \
for (El = Head; El; El = El->next)

/**
 * Safe iteration over chain elements.
 * Current element may deleted.
 */
#define chain_for_each_safe(Head, El, Next)       \
for (El = Head, Next = (El) ? (El)->next : NULL;  \
     El;                                          \
     El = Next, Next = (Next) ? (Next)->next : NULL)

/**
 * Iterate over chain elements except @a Ex starting from @a Hd.
 * @a Ex must be valid non-NULL pointer.
 */
#define chain_for_each_except(Hd, El, Ex) \
for (El = (Hd == Ex) ? (Hd)->next : (Hd); \
     El;                                  \
     El = ((El)->next == Ex) ? (Ex)->next : (El)->next)

#endif /* _CHAIN_H */
