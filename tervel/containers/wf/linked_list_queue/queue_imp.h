#ifndef __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_QUEUE__IMP_H_
#define __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_QUEUE__IMP_H_

#include <tervel/util/progress_assurance.h>


namespace tervel {
namespace containers {
namespace wf {

template<typename T>
bool
Queue<T>::Node::on_watch(std::atomic<void *> *address, void *expected) {
  access();
  if (address->load() == expected) {
    return true;
  } else {
    unaccess();
    return false;
  }
}

// template<typename T>
// void
// Queue<T>::Node::on_unwatch(std::atomic<void *> *address, void *expected) {
//   // Do not use. //
//   return;
// }

template<typename T>
bool
Queue<T>::Node::on_is_watched() {
  return is_accessed();
}

template<typename T>
void
Queue<T>::Accessor::uninit() {
  // TODO: Implement
  if (ptr_ != nullptr) {
    ptr_->unaccess();
  }
  ptr_ = nullptr;

  if (ptr_next_ != nullptr) {
    ptr_next_->unaccess();
  }
  ptr_next_ = nullptr;
};

template<typename T>
void
Queue<T>::Accessor::unaccess_ptr_only() {
  // TODO: Implement
  if (ptr_ != nullptr) {
    ptr_->unaccess();
  }
  ptr_ = nullptr;
};

template<typename T>
bool
Queue<T>::Accessor::init(Node *node, std::atomic<Node *> *address) {
  // TODO: Implement
  assert(node != nullptr);
  bool res;

  res = tervel::util::memory::hp::HazardPointer::watch(kSlot, node,
    reinterpret_cast<std::atomic<void *> *>(address), node);
  if (res == false) { return false; }
  tervel::util::memory::hp::HazardPointer::unwatch(kSlot);
  ptr_ = node;

  Node *next = node->next();
  if (next != nullptr) {
    res = tervel::util::memory::hp::HazardPointer::watch(kSlot, next,
      reinterpret_cast<std::atomic<void *> *>(address), node);
  }
  if (res == false) {
    unaccess_ptr_only();
    return false;
  }
  tervel::util::memory::hp::HazardPointer::unwatch(kSlot);
  ptr_next_ = next;

  return true;
};

template<typename T>
Queue<T>::Queue() {
  Node * node = new Node();
  head_.store(node);
  tail_.store(node);
}


template<typename T>
Queue<T>::~Queue() {
  // TODO: Implement
  while (!empty()) {
    Accessor a;
    if (!dequeue(a)) {
      return;
    }
  }
}


template<typename T>
bool
Queue<T>::enqueue(T &value) {
  // TODO: Implement
  tervel::util::ProgressAssurance::check_for_announcement();
  util::ProgressAssurance::Limit progAssur;


  Node *node = new Node(value);
  while (!progAssur.isDelayed()) {
    Accessor access;
    Node *last = tail();
    if (access.init(last, &tail_) == false) {
      continue;
    }
    Node *next = access.ptr_next();

    if (last == tail()) {
      if (next == nullptr) {
        if (last->set_next(next, node)) {
          set_tail(last, node);
          size(1);
          return true;
        }
      } else {
        set_tail(last, next);
      }
    }
  }
  delete node;
  EnqueueOp *op = new EnqueueOp(this, value);
  tervel::util::ProgressAssurance::make_announcement(op);
  op->safe_delete();
  size(1);
  return true;
}

template<typename T>
bool
Queue<T>::dequeue(Accessor &access) {
  // TODO: Implement
  tervel::util::ProgressAssurance::check_for_announcement();
  tervel::util::ProgressAssurance::Limit progAssur;

  while (!progAssur.isDelayed()) {
    Node *first = head();
    Node *last = tail();
    if (access.init(first, &head_) == false) {
      continue;
    }
    Node *next = access.ptr_next();

    if (first == head()) {
      if (first == last) {
        if (next == nullptr) {
          return false;
        }
        set_tail(last, next);
      } else {
        assert(next != nullptr);
        if (set_head(first, next)) {
          size(-1);
          access.unaccess_ptr_only();
          first->safe_delete();
          return true;
        }
      }
    }
    access.uninit();
  }

  DequeueOp *op = new DequeueOp(this);
  tervel::util::ProgressAssurance::make_announcement(op);
  bool res = op->result(access);
  op->safe_delete();
  if (res)
    size(-1);
  return res;
};


template<typename T>
bool
Queue<T>::empty() {
  // TODO: Implement
  while (true) {
    Accessor access;
    Node *first = head();
    if (access.init(first, &head_) == false) {
      continue;
    }
    return access.ptr_next() == nullptr;
  }
}

}  // namespace wf
}  // namespace containers
}  // namespace tervel
#endif  // __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_QUEUE__IMP_H_