#include "include/lib.h"

#include <cassert>
#include <iostream>
#include <vector>

// See comments in the lib.h.
std::shared_ptr<CoroBase> this_coro{};
std::jmp_buf sched_ctx{};
std::jmp_buf start_point{};

void CoroBody(int signum) {
  std::shared_ptr<CoroBase> c = this_coro->GetPtr();
  this_coro.reset();

  if (setjmp(c->ctx) == 0) {
    longjmp(start_point, 1);
  }

  c->ret = c->Run();
  c->is_returned = true;
  c.reset();
  longjmp(sched_ctx, 1);
}

std::shared_ptr<CoroBase> CoroBase::GetPtr() { return shared_from_this(); }

void CoroBase::SetToken(std::shared_ptr<Token> token) { this->token = token; }

void CoroBase::Resume() {
  this_coro = this->GetPtr();
  assert(!this_coro->IsReturned());
  if (setjmp(sched_ctx) == 0) {
    longjmp(this_coro->ctx, 1);
  }
  this_coro.reset();
}

int CoroBase::GetRetVal() const {
  assert(IsReturned());
  return ret;
}

bool CoroBase::IsParked() const { return token != nullptr && token->parked; }

CoroBase::~CoroBase() {
  // The coroutine must be returned if we want to restart it.
  // We can't just Terminate() it because it is the runtime responsibility to
  // decide, in which order the tasks should be terminated.
  assert(IsReturned());
}

std::string_view CoroBase::GetName() const { return name; }

bool CoroBase::IsReturned() const { return is_returned; }

extern "C" void CoroYield() {
  assert(this_coro);
  if (setjmp(this_coro->ctx) == 0) {
    longjmp(sched_ctx, 1);
  }
}

void CoroBase::Terminate() {
  int tries = 0;
  while (!IsReturned()) {
    ++tries;
    Resume();
    assert(tries < 10000000 &&
           "coroutine is spinning too long, possible wrong terminating order");
  }
}

void Token::Reset() { parked = false; }

void Token::Park() {
  parked = true;
  CoroYield();
}

void Token::Unpark() { parked = false; }
