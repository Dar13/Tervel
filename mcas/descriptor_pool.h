#ifndef MCAS_DESCRIPTOR_POOL_H_
#define MCAS_DESCRIPTOR_POOL_H_

#define DEBUG_POOL true

#include <atomic>
#include <utility>

#include <assert.h>
#include <stdint.h>

#include "pool_element.h"

namespace ucf {
namespace thread {

class Descriptor;

namespace rc {

class PoolManager;

/**
 * If true, then DescriptorPool shouldn't reuse old pool elements when being
 * asked, even if it's safe to fo so. Instead, elements should be stock-piled
 * and left untouched when they're returned to the pool. This allows the user to
 * view associations. Entirely for debug purposes.
 *
 * TODO(carlos): move this to member var
 */
constexpr bool NO_REUSE_MEM = false;

/**
 * Defines a pool of descriptor objects which is used to allocate descriptors
 * and to store them while they are not safe to delete.
 *
 * The pool is represented as two linked lists of descriptors: one for safe
 * elements and one for unsafe elements. The safe pool is known to only contain
 * items owned by the thread owning the DescriptorPool object, and the unsafe
 * pool contains items where other threads may still hold refrences to them.
 *
 * Further, the pool object has a parent who is shared amongst other threads.
 * When a pool is to be destroyed, it sends its remaining elements to the
 * parent, relinquishing ownership of said elements. A top-level pool has a null
 * parent. At the moment, it only makes sense to have a single top-level parent
 * representing the central pool for all threads, and several local pools for
 * each thread.
 */
class DescriptorPool {
 public:
  DescriptorPool(int pool_id, PoolManager *manager, int prefill=4)
      : pool_id_(pool_id)
      , manager_(manager) {
    this->reserve(prefill);
  }
  ~DescriptorPool() { this->send_to_manager(); }

  /**
   * Constructs and returns a descriptor. Arguments are forwarded to the
   * constructor of the given descriptor type. User should call free_descriptor
   * on the returned pointer when they are done with it to avoid memory leaks.
   */
  template<typename DescrType, typename... Args>
  Descriptor * get_descriptor(Args&&... args);

  /**
   * Once a user is done with a descriptor, they should free it with this
   * method.
   *
   * @param descr The descriptor to free.
   */
  void free_descriptor(Descriptor *descr);

  /**
   * Assures that the pool has at least num_descriptor elements to spare so that
   * calls to the main allocator can be avoided.
   */
  void reserve(int num_descriptors);


 private:
  // -------------------------
  // FOR DEALING WITH ELEMENTS
  // -------------------------

  /**
   * If the given descriptor was allocated through a DescriptorPool, then it has
   * an associated PoolElement header. This methods returns that PoolElement.
   *
   * Use with caution as Descriptors not allocated from a pool will not have an
   * associated header, and, thus, the returned value will be to some random
   * place in memory.
   */
  static PoolElement * get_elem_from_descriptor(Descriptor *descr);

  /**
   * Gets a free element. The local pool is checked for one first, then the
   * manager if there are no local ones, and if all else fails, a new one is
   * allocated using new.
   *
   * @param allocate_new If true and there are no free elements to retrieve from
   *   the pool, a new one is allocated. Otherwise, nullptr is returned.
   */
  PoolElement * get_from_pool(bool allocate_new=true);


  // -------------------------
  // FOR DEALING WITH MANAGERS
  // -------------------------

  /**
   * Sends all elements managed by this pool to the parent pool. Same as:
   *   send_safe_to_parent();
   *   send_unsafe_to_parent();
   * TODO(carlos): what should happen if parent_ is null?
   */
  void send_to_manager();

  /**
   * Sends the elements from the safe pool to the parent pool's safe pool.
   */
  void send_safe_to_manager();

  /**
   * Sends the elements from the unsafe pool to the parent pool's unsafe pool.
   */
  void send_unsafe_to_manager();


  // --------------------------------
  // DEALS WITH SAFE AND UNSAFE LISTS
  // --------------------------------

  /**
   * Releases the descriptor back to the safe pool. Caller relinquishes
   * ownership of descriptor. It's expected that the given descriptor was taken
   * from this pool to begin with.
   *
   * Adding a descriptor to the safe pool calls its 'advance_return_to_pool'
   * method and its destructor.
   */
  void add_to_safe(Descriptor* descr);

  /**
   * Releases the descriptor back to the unsafe pool. Caller relinquishes
   * ownership of descriptor. It's expected that the given descriptor was taken
   * from this pool to begin with.
   *
   * See notes on add_to_safe()
   */
  void add_to_unsafe(Descriptor* descr);

  /**
   * Clear all elements from the safe pool.
   */
  void free_safe();

  /**
   * Try to move elements from the unsafe pool to the safe pool.
   */
  void try_free_unsafe(bool dont_check=false);


  /**
   * Index into this pool's manager's pool array corresponding to this pool.
   */
  int pool_id_;

  /**
   * This pool's manager.
   */
  PoolManager *manager_;

  /**
   * A linked list of pool elements.  One can be assured that no thread will try
   * to access the descriptor of any element in this pool. They can't be freed
   * as some threads may still have access to the element itself and may try to
   * increment the refrence count.
   */
  PoolElement *safe_pool_ {nullptr};

  /**
   * A linked list of pool elements. Elements get released to this pool when
   * they're no longer needed, but some threads may still try to access the
   * descriptor in the element. After some time has passed, items generally move
   * from this pool to the safe_pool_
   */
  PoolElement *unsafe_pool_ {nullptr};

#ifdef DEBUG_POOL
  // TODO(carlos) what, exactly, do these keep track of?
  uint64_t safe_pool_count_ {0};
  uint64_t unsafe_pool_count_ {0};
#endif

};


// IMPLEMENTATIONS
// ===============

template<typename DescrType, typename... Args>
Descriptor * DescriptorPool::get_descriptor(Args&&... args) {
  PoolElement *elem = this->get_from_pool();
  elem->init_descriptor<DescrType>(std::forward<Args>(args)...);
  return elem->descriptor();
}

}  // namespace rc
}  // namespace thread
}  // namespace ucf

#endif  // MCAS_DESCRIPTOR_POOL_H_

