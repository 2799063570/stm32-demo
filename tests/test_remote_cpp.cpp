#include "aubo_remote_bridge/remote_core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

namespace abr = aubo_remote_bridge;
static std::vector<uint8_t> readFile(const char* name) {
  std::ifstream file(name, std::ios::binary); assert(file.good());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
int main() {
  // 使用MCU真实C测试导出的报文，检查C++编码/解码与固件逐字节一致。
  abr::Frame poll{}; poll.Type = REMOTE_POLL; poll.Sequence = 42; poll.Length = 4;
  Remote_PutU32(poll.Payload, 0x10203040);
  assert(abr::encode(poll) == readFile("remote_poll_frame.bin"));
  abr::Reply ack;
  const std::array<int16_t, 6> joints{{0, 1571, -1571, 3142, -3142, 500}};
  assert(abr::encode(abr::feedback(0x10203040, 42, 1, 63, joints, ack, REMOTE_UNKNOWN, 0)) == readFile("remote_feedback_frame.bin"));
  Remote_ParserTypeDef parser{}; abr::Frame input_frame{};
  int decoded = 0;
  for (auto byte : readFile("remote_input_frame.bin")) decoded += Remote_ParseByte(&parser, byte, &input_frame);
  assert(decoded == 1 && input_frame.Length == 18 && Remote_GetI16(input_frame.Payload + 8) == 1000);
  abr::Poller core(123); abr::Frame request{}; abr::Input input;
  assert(core.request(0., request));
  input_frame.Sequence = request.Sequence; Remote_PutU32(input_frame.Payload, 123);
  assert(core.receive(input_frame, .01, input) && core.online(.01));
  abr::Request command;
  input.request_id = 7; input.command = REMOTE_HOME;
  assert(!core.command(input, command));             // 初始缓存只作基线
  input.request_id = 8;
  assert(core.command(input, command) && command.id == 8);
  assert(!core.command(input, command));             // 持续重复INPUT不重复派发
  assert(core.request(.03, request));
  assert(core.request(.14, request));                // 过期后重新轮询
  input_frame.Sequence = request.Sequence;
  Remote_PutU32(input_frame.Payload, core.session());
  Remote_PutU16(input_frame.Payload + 16, 8); input_frame.Payload[15] = REMOTE_HOME;
  assert(core.receive(input_frame, .15, input));
  assert(!core.command(input, command));             // 重连/超时恢复不重放旧动作
  assert(!core.receive(input_frame, .16, input));     // 同一回复只接收一次
  assert(core.request(.17, request));
  input_frame.Sequence = request.Sequence; input_frame.Payload[13] = 2;
  Remote_PutU16(input_frame.Payload + 8, 0); Remote_PutU16(input_frame.Payload + 10, 0);
  assert(core.receive(input_frame, .18, input));
  const auto before_reset = core.session();
  assert(core.request(.20, request) && core.session() != before_reset); // MCU重新校准换会话
  abr::Ledger ledger; abr::Reply result;
  const abr::Request home{1, 10, REMOTE_HOME};
  assert(ledger.find(home, result) == abr::Ledger::Missing);
  ledger.save({home, REMOTE_ACCEPTED, "已接收"});
  assert(ledger.find(home, result) == abr::Ledger::Duplicate && result.state == REMOTE_ACCEPTED);
  ledger.save({home, REMOTE_SUCCEEDED, "已完成"});
  assert(ledger.find(home, result) == abr::Ledger::Duplicate && result.state == REMOTE_SUCCEEDED);
  assert(ledger.find({1, 10, REMOTE_ZERO}, result) == abr::Ledger::Conflict);
  assert(ledger.find({2, 10, REMOTE_HOME}, result) == abr::Ledger::Missing);
  std::cout << "PASS: C/C++ wire agreement, polling/session freshness, request baseline/deduplication/ledger\n";
  return 0;
}
