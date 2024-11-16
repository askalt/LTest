#pragma once

#include <gmock/gmock.h>

<<<<<<< HEAD:src/test/runtime/stackfulltask_mock.h
#include "include/scheduler.h"
=======
#include <utility>

#include "scheduler.h"
>>>>>>> 43c4393 (erase build logic from verify script):test/runtime/stackfulltask_mock.h

class MockStackfulTask : public StackfulTask {
 public:
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(bool, IsReturned, (), (override));
  MOCK_METHOD(int, GetRetVal, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
};
