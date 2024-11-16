#include "../specs/coroutine_queue.h"

#include <coroutine>
#include <memory>
#include <optional>
#include <queue>

#include "../../runtime/include/verifying.h"

struct CoroutineQueue {
  struct SendPromise;
  struct ReceivePromise;

  CoroutineQueue()
      : receivers(std::queue<ReceivePromise>()),
        senders(std::queue<SendPromise>()) {}

  SendPromise send(int elem) { return SendPromise(elem, *this); }
  ReceivePromise receive() { return ReceivePromise(*this); }

  //  ReceivePromise Receive() { return ReceivePromise(*this); }

  size_t ReceiversCount() { return receivers.size(); }

  struct ReceivePromise {
    explicit ReceivePromise(CoroutineQueue& queue) : queue(queue) {
      elem = std::make_shared<std::optional<int>>(
          std::optional<int>(std::nullopt));
    }
    bool await_ready() { return false; }

    non_atomic bool await_suspend(std::coroutine_handle<> h) {
      std::cout << "size: " << queue.senders.size() << std::endl;
      if (!queue.senders.empty()) {
        std::cout << "size: " << queue.senders.size() << std::endl;
        //        std::cout << "magic happened" << std::endl;
        SendPromise send_req = queue.senders.back();
        CoroYield();
        queue.senders.pop();
        *elem = send_req.elem;
        assert(!send_req.sender.done());
        send_req.sender();
        return false;
      } else {
        receiver = h;
        queue.receivers.push(*this);
        return true;
      }
    }

    int await_resume() {
      assert(elem.get()->has_value());
      return **elem;
    }

    std::coroutine_handle<> receiver;
    std::shared_ptr<std::optional<int>> elem;
    CoroutineQueue& queue;
  };

  struct SendPromise {
    SendPromise(int elem, CoroutineQueue& queue) : elem(elem), queue(queue) {}

    bool await_ready() { return false; }

    non_atomic bool await_suspend(std::coroutine_handle<> h) {
      if (!queue.receivers.empty()) {
        ReceivePromise receiver = queue.receivers.back();
        CoroYield();
        queue.receivers.pop();
        *(receiver.elem) = elem;
        assert(!receiver.receiver.done());
        receiver.receiver();
        return false;
      } else {
        sender = h;
        queue.senders.push(*this);
        return true;
      }
    }

    int await_resume() { return 0; }

    std::coroutine_handle<> sender;
    int elem;
    CoroutineQueue& queue;
  };

  void Reset() {
    receivers = {};
    senders = {};
  }

 private:
  std::queue<ReceivePromise> receivers;
  std::queue<SendPromise> senders;
};

// Arguments generator.
auto generateInt(size_t unused_param) {
  return ltest::generators::makeSingleArg(rand() % 10 + 1);
}

target_method_dual(ltest::generators::genEmpty, CoroutineQueue::ReceivePromise,
                   ReceivePromise, CoroutineQueue, receive);

target_method_dual(generateInt, CoroutineQueue::SendPromise, SendPromise,
                   CoroutineQueue, send, int);

// Specify target structure and it's sequential specification.
using spec_t = ltest::SpecDual<CoroutineQueue, spec::CoroutineQueue<>>;

LTEST_ENTRYPOINT_DUAL(spec_t);
