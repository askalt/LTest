#include <functional>
#include <map>
#include <queue>

#include "../../runtime/include/lincheck_dual.h"
namespace spec {

template <typename PushArgTuple = std::tuple<int>, std::size_t ValueIndex = 0>
struct CoroutineQueue {
  struct SendPromise;
  struct ReceivePromise;

  explicit CoroutineQueue(size_t buffer_size)
      : receivers(std::deque<ReceivePromise*>()),
        senders(std::deque<SendPromise*>()),
        buffer_size(buffer_size),
        buffer(std::queue<int>()) {}

  CoroutineQueue() : CoroutineQueue(6) {}

  SendPromise Send(int elem) { return SendPromise(elem, this); }

  ReceivePromise Receive() { return ReceivePromise(this); }

  size_t ReceiversCount() { return receivers.size(); }

  struct ReceivePromise {
    explicit ReceivePromise(CoroutineQueue* queue) : queue(queue) {
      elem = std::make_shared<std::optional<int>>(std::nullopt);
    }
    bool await_ready() { return false; }

    bool await_suspend(std::coroutine_handle<> h) {
      assert(queue != nullptr);
      if (!queue->buffer.empty()) {
        int elem_from_buffer = queue->buffer.front();
        queue->buffer.pop();
        *elem = elem_from_buffer;
        return false;
      } else if (!queue->senders.empty()) {
        SendPromise* send_req = queue->senders.front();
        queue->senders.pop_front();
        *elem = send_req->elem;
        send_req->sender();
        return false;
      } else {
        receiver = h;
        queue->receivers.push_back(this);
        return true;
      }
    }

    std::optional<int> await_resume() { return *elem; }

    std::coroutine_handle<> receiver;
    std::shared_ptr<std::optional<int>> elem;
    CoroutineQueue* queue;
  };

  struct SendPromise {
    SendPromise(int elem, CoroutineQueue* queue) : elem(elem), queue(queue) {}

    bool await_ready() { return false; }
    bool await_suspend(std::coroutine_handle<> h) {
      assert(queue != nullptr);
      if (queue->buffer.size() != queue->buffer_size) {
        queue->buffer.push(elem);
        return false;
      } else if (!queue->receivers.empty()) {
        ReceivePromise* receiver = queue->receivers.front();
        queue->receivers.pop_front();
        *(receiver->elem) = elem;
        receiver->receiver();
        return false;
      } else {
        sender = h;
        queue->senders.push_back(this);
        return true;
      }
    }

    std::optional<int> await_resume() { return 0; }

    std::coroutine_handle<> sender;
    int elem;
    CoroutineQueue* queue;
  };

  CoroutineQueue(const CoroutineQueue& other) {
    receivers = std::deque<ReceivePromise*>();
    senders = std::deque<SendPromise*>();
    buffer = other.buffer;
    buffer_size = other.buffer_size;

    //      assert(other.receivers.empty());
    //      assert(other.senders.empty());
    for (ReceivePromise* receiver : other.receivers) {
      ReceivePromise* new_receiver = new ReceivePromise(this);
      new_receiver->receiver = receiver->receiver;
      receivers.push_back(new_receiver);
    }
    for (SendPromise* sender : other.senders) {
      SendPromise* new_sender = new SendPromise(sender->elem, this);
      new_sender->sender = sender->sender;
      senders.push_back(new_sender);
    }
  }

  //  CoroutineQueue& operator=(const CoroutineQueue& other) {
  //    receivers = std::deque<ReceivePromise*>();
  //    senders = std::deque<SendPromise*>();
  //    buffer = other.buffer;
  //    buffer_size = other.buffer_size;
  //
  //    assert(other.receivers.empty());
  //    assert(other.senders.empty());
  //    return *this;
  //  }

  CoroutineQueue& operator=(CoroutineQueue&& other) {
    receivers = std::deque<ReceivePromise*>();
    senders = std::deque<SendPromise*>();
    buffer = other.buffer;
    buffer_size = other.buffer_size;

    for (ReceivePromise* receiver : other.receivers) {
      ReceivePromise* new_receiver = new ReceivePromise(this);
      new_receiver->receiver = receiver->receiver;
      receivers.push_back(new_receiver);
    }
    for (SendPromise* sender : other.senders) {
      SendPromise* new_sender = new SendPromise(sender->elem, this);
      new_sender->sender = sender->sender;
      senders.push_back(new_sender);
    }
    return *this;
  }

  CoroutineQueue(CoroutineQueue&& other) {
    receivers = std::deque<ReceivePromise*>();
    senders = std::deque<SendPromise*>();
    buffer = other.buffer;
    buffer_size = other.buffer_size;

    for (ReceivePromise* receiver : other.receivers) {
      ReceivePromise* new_receiver = new ReceivePromise(this);
      new_receiver->receiver = receiver->receiver;
      receivers.push_back(new_receiver);
    }
    for (SendPromise* sender : other.senders) {
      SendPromise* new_sender = new SendPromise(sender->elem, this);
      new_sender->sender = sender->sender;
      senders.push_back(new_sender);
    }
  }

  static auto GetMethods() {
    LinearizabilityDualChecker<CoroutineQueue<>>::BlockingMethodFactory
        receive =
            [](CoroutineQueue<>* c,
               [[maybe_unused]] void* args) -> std::shared_ptr<BlockingMethod> {
      return std::shared_ptr<BlockingMethod>(
          new BlockingMethodWrapper<CoroutineQueue<>::ReceivePromise>(
              c->Receive()));
    };

    LinearizabilityDualChecker<CoroutineQueue<>>::BlockingMethodFactory send =
        [](CoroutineQueue<>* c, void* args) -> std::shared_ptr<BlockingMethod> {
      auto real_args = reinterpret_cast<std::tuple<int>*>(args);
      return std::shared_ptr<BlockingMethod>(
          new BlockingMethodWrapper<CoroutineQueue<>::SendPromise>(
              c->Send(std::get<0>(*real_args))));
    };

    return LinearizabilityDualChecker<CoroutineQueue<>>::MethodMap{
        {"receive", receive},
        {"send", send},
    };
  }

 private:
  std::deque<ReceivePromise*> receivers;
  std::deque<SendPromise*> senders;
  size_t buffer_size{};
  std::queue<int> buffer;
};

};  // namespace spec