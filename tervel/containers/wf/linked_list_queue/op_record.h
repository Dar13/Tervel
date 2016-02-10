
#ifndef __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_OP_RECORD_H_
#define __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_OP_RECORD_H_

#include <tervel/util/util.h>
#include <tervel/util/progress_assurance.h>
#include <tervel/util/memory/hp/hp_element.h>
#include <tervel/util/memory/hp/hazard_pointer.h>

#include <tervel/containers/wf/linked_list_queue/queue.h>

namespace tervel {
namespace containers {
namespace wf {

template<typename T>
class Queue<T>::Op : public util::OpRecord {
 public:
  static const tervel::util::memory::hp::HazardPointer::SlotID kSlot =
    tervel::util::memory::hp::HazardPointer::SlotID::SHORTUSE2;

  class Helper : public tervel::util::memory::hp::Element {
   public:
    Helper(Op * op) : op_(op) {};

    bool on_watch(std::atomic<void *> *address, void *expected) {
      bool res = tervel::util::memory::hp::HazardPointer::watch(kSlot, op_,
        address, expected);
      if (res == false) { return false; }

      remove(reinterpret_cast<std::atomic<Node *> *>(address));

      tervel::util::memory::hp::HazardPointer::unwatch(kSlot);
      return false;
    };

    bool is_valid() {
      return op_->helper() == this;
    };

    bool remove(std::atomic<Node *> *address) {
      Node *temp = reinterpret_cast<Node *>(this);
      bool res = op_->associate(this);
      if (res) {
        address->compare_exchange_strong(temp, newValue_);
      } else {
        address->compare_exchange_strong(temp, expValue_);
      }
      return res;
    };

    Op * const op_;
    Node * newValue_;
    Node * expValue_;
    void expValue(Node *e) { expValue_ = e; };
    void newValue(Node *e) { newValue_ = e; };
  };

  Op(Queue<T> *queue) : queue_(queue) {};

  ~Op() {
    Helper *h = helper();
    assert(h != nullptr);
    if (h != fail_val_) {
      delete h;
    }
  };

  void fail() {
    associate(fail_val_);
  };

  bool notDone() {
    return helper() == nullptr;
  };

  bool on_is_watched() {
    Helper *h = helper_.load();
    assert(h != nullptr);
    if (h != fail_val_) {
      bool res = tervel::util::memory::hp::HazardPointer::is_watched(h);
      return res;
    }
    return false;
  };

  std::atomic<Helper *> helper_{nullptr};
  Helper * helper() { return helper_.load(); };
  bool associate(Helper *h) {
    Helper * temp = nullptr;
    bool res = helper_.compare_exchange_strong(temp, h);
    return res || helper() == h;
  };

  static constexpr Helper * fail_val_ = reinterpret_cast<Helper *>(0x1L);

  Queue<T> * const queue_ {nullptr};
};


template<typename T>
class Queue<T>::DequeueOp : public Queue<T>::Op {
 public:
  DequeueOp(Queue<T> *queue)
    : Op(queue) {};

  bool result(Accessor &access) {
    typename Queue<T>::Op::Helper *helper = Op::helper();
    if (helper == Op::fail_val_) {
      return false;
    } else {
      access.set_ptr_next(helper->newValue_);
      return true;
    }
  };

  void help_complete() {
    typename Queue<T>::Op::Helper *helper = new typename Queue<T>::Op::Helper(this);
    Node *node_helper = reinterpret_cast<Node *>(helper);

    while (Op::notDone()) {
      Accessor access;

      Node *first = Op::queue_->head();
      Node *last = Op::queue_->tail();
      if (access.init(first, &Op::queue_->head_) == false) {
        continue;
      }
      Node *next = access.ptr_next();

      if (first == Op::queue_->head()) {
        if (first == last) {
          if (next == nullptr) {
            Op::fail();
            return;
          }
          Op::queue_->set_tail(last, next);
        } else {
          assert(next != nullptr);

          helper->expValue(first);
          helper->newValue(next);

          if (Op::queue_->set_head(first, node_helper)) {
            if (helper->remove(&Op::queue_->head_)) {
              access.unaccess_ptr_only();
              first->safe_delete();
              access.set_ptr_next(nullptr);
            } else {
              helper->safe_delete();
            }
            return;
          }
        }
      }
    }  // End while
    delete helper;
  };

  T value_;
  // DISALLOW_COPY_AND_ASSIGN(EnqueueOp);
};  // class


template<typename T>
class Queue<T>::EnqueueOp : public Queue<T>::Op {
 public:
  EnqueueOp(Queue<T> *queue, T &value)
    : Op(queue) {
      value_ = value;
  };

  void help_complete() {
    Node *node = new Node(value_);
    typename Queue<T>::Op::Helper *helper = new typename Queue<T>::Op::Helper(this);
    helper->newValue(node);
    Node *node_helper = reinterpret_cast<Node *>(helper);
    while (Op::notDone()) {
      Accessor access;
      Node *last = Op::queue_->tail();
      if (access.init(last, &Op::queue_->tail_) == false) {
        continue;
      }
      Node *next = access.ptr_next();

      if (last == Op::queue_->tail()) {
        if (next == nullptr) {
          helper->expValue(next);
          if (last->set_next(next, node_helper)) {
            if (helper->remove(&last->next_)) {
              Op::queue_->set_tail(last, node);
            } else {
              helper->safe_delete();
            }
            return;
          }
        } else {
          Op::queue_->set_tail(last, next);
        }
      }
    }
    delete node;
    delete helper;
  };

  T value_;
  // DISALLOW_COPY_AND_ASSIGN(EnqueueOp);
};  // class

}  // namespace wf
}  // namespace containers
}  // namespace tervel

#endif  // __TERVEL_CONTAINERS_WF_LINKED_LIST_QUEUE_OP_RECORD_H_