#include "descriptor_pool.h"

namespace ucf {
namespace thread {
namespace rc {

// DescriptorPool Implementations
// ==============================
void DescriptorPool::add_to_safe(Descriptor *descr) {
  PoolElem *p = getPoolElemRef(descr);

#ifdef DEBUG_POOL
  p->header_.free_count_.fetch_add(1);
  assert(p->header_.free_count_.load() == p->header_.allocation_count_.load());
  safe_pool_count_++;
#endif

  p->next_ = safe_pool_;
  safe_pool_ = p;
}

PoolElem * DescriptorPool::get_elem_from_descriptor(Descriptor *descr) {
  return reinterpret_cast<PoolElem *>(descr) - 1;
}

PoolElem * DescriptorPool::get_from_pool(bool allocate_new) {
  // move any objects from the unsafe list to the safe list so that it's more
  // likely the safe list has something to take from.
  this->try_free_unsafe();

  PoolElem *ret {nullptr};

  // safe pool has something in it. pop the next item from the head of the list.
  if (!NO_REUSE_MEM && safe_pool_ != nullptr) {
    ret = safe_pool_;
    safe_pool_ = safe_pool_->next_;
    ret->next_ = nullptr;

#ifdef DEBUG_POOL
    // update counters to denote that an item was taken from the pool
    assert(ret->header_.free_count_.load() ==
        ret->header_.allocation_count_.load());
    ret->header_.allocation_count_.fetch_add(1);
    safe_pool_count_ -= 1;
#endif
  }

  // allocate a new element if needed
  // TODO(carlos) the way ownership semantics work here, the caller of this
  // function owns any returned pool elements. This leaves open the possibility
  // that the caller may leak memory. Alternative is that this pool object owns
  // the memory, but there may not be any performant way to do that.
  if (ret == nullptr && allocate_new) {
    ret = new PoolElem();
  }

  return ret;
}


}  // namespace rc
}  // namespace thread
}  // namespace ucf
