#include "tervel/util/progress_assurance.h"
#include "tervel/util/memory/hp/hazard_pointer.h"

namespace tervel {
namespace util {

bool OpRecord::didSucceed() {
  if (this->helper != nullptr) {
    if (this->helper->op == this)
      return true;
  }
  return false;
}

/**
 * @return Returns if the given thread enqueued a value or successfully
 *    associated
 */
void ProgressAssurance::enqueuer_check_for_announcement() {
  // Internally, delay_count is incremented and set to 0 when ever HELP_DELAY
  // is reached
  size_t delay_count = tl_thread_info->delay_count(HELP_DELAY);
  if (delay_count == 0) {
    // Internally, help_id is incremented and wrapped to number of threads.
    size_t help_id = tl_thread_info->help_id(num_threads_);
    OpRecord *op = op_table_[help_id].load();
    if (op != nullptr) {
      std::atomic<void *> *address = reinterpret_cast<std::atomic<void *> *>(
              &(op_table_[help_id]));

      typedef memory::hp::HazardPointer::SlotID SlotID;
      SlotID pos = SlotID::PROG_ASSUR;
      bool res = memory::hp::HazardPointer::watch(pos, op, address, op);
      if (res) {
        return op->help_complete();
      }
    }
  }
}

template<class T>
bool ProgressAssurance::dequeuer_check_for_announcement(T *val) {
  // Internally, delay_count is incremented and set to 0 when ever HELP_DELAY
  // is reached
  size_t delay_count = tl_thread_info->delay_count(HELP_DELAY);
  if (delay_count == 0) {
    // Internally, help_id is incremented and wrapped to number of threads.
    size_t help_id = tl_thread_info->help_id(num_threads_);
    OpRecord *op = op_table_[help_id].load();
    if (op != nullptr) {
      std::atomic<void *> *address = reinterpret_cast<std::atomic<void *> *>(
              &(op_table_[help_id]));

      typedef memory::hp::HazardPointer::SlotID SlotID;
      SlotID pos = SlotID::PROG_ASSUR;
      bool res = memory::hp::HazardPointer::watch(pos, op, address, op);
      if (res) {
        *val = op->help_complete();
        return true;
      }
    }
  }
  return false;
}

void ProgressAssurance::p_make_announcement(OpRecord *op, int tid) {
  op_table_[tid].store(op);
  op->help_complete();
  op_table_[tid].store(nullptr);
}

}  // namespace memory
}  // namespace tervel
