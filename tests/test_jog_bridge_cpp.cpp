#include <cassert>
#include <iostream>
#include "aubo_remote_bridge/jog_remote_core.hpp"
extern "C" {
#include "JogRemote.h"
}
namespace abr = aubo_remote_bridge;

struct Loopback {
  Jog_Remote mcu{};
  abr::JogPoller poller{100};
  uint32_t now = 0;
  uint16_t request = 0;
  uint8_t command = 0, ack = 0, controller = REMOTE_JOG_DISABLED, flags = 5;
  Loopback() {
    Jog_Init(&mcu);
    sample(40);
    exchange(); sample(1);
    assert(mcu.Ready && Jog_Online(&mcu, now) && !mcu.NeedNeutral);
  }
  void sample(unsigned count, uint16_t x = 2048, uint8_t key = 0) {
    for (unsigned i = 0; i < count; ++i) {
      now += 5; Jog_Feed(&mcu, x, 2048, key, 1, now);
    }
  }
  RemoteJog_Input exchange() {
    abr::Frame poll{}, reply{}, feedback{};
    RemoteJog_Input input{};
    assert(poller.poll(now / 1000., poll));
    assert(Jog_OnFrame(&mcu, &poll, now, &reply));
    Jog_TxResult(&mcu, 1);
    assert(poller.receive(reply, now / 1000. + .001, input));
    RemoteJog_Feedback value{};
    value.session = input.session; value.request_id = request; value.command = command;
    value.ack_state = ack; value.controller_state = controller; value.flags = flags;
    value.home_state = 255; value.home_error = 65535;
    RemoteJog_EncodeFeedback(&value, reply.Sequence, &feedback);
    Jog_OnFrame(&mcu, &feedback, now + 1, &reply);
    return input;
  }
};

void delayedStateAndRealFirmware() {
  Loopback loop;
  abr::JogEnableConfirmation confirmation;
  loop.mcu.Page = JOG_PAGE_DRIVE;
  assert(Jog_Request(&loop.mcu, REMOTE_JOG_ENABLE, loop.now));
  loop.request = loop.mcu.RequestId; loop.command = REMOTE_JOG_ENABLE;
  loop.ack = REMOTE_ACCEPTED; loop.flags = 13;  // input在线、状态在线、确认忙。
  loop.sample(5); loop.exchange();
  confirmation.begin(10, loop.now / 1000.);
  assert(confirmation.update(loop.now / 1000. + .001, 10, true, true) == abr::JogEnableConfirmation::Waiting);
  loop.sample(5);
  assert(confirmation.update(loop.now / 1000., 11, false, true) == abr::JogEnableConfirmation::Waiting);
  loop.exchange();
  assert(loop.mcu.EnablePending && !loop.mcu.Authorized);
  loop.sample(5);
  assert(confirmation.update(loop.now / 1000., 12, true, true) == abr::JogEnableConfirmation::Confirmed);
  loop.ack = REMOTE_SUCCEEDED; loop.flags = 7; loop.controller = REMOTE_JOG_IDLE;
  loop.exchange();
  assert(loop.mcu.Authorized && !loop.mcu.EnablePending);
  loop.sample(5, 4095, JOG_KEY_MENU);
  auto input = loop.exchange();
  assert(input.flags == 7 && input.axis[0] == 1000);
  loop.sample(1, 4095);
  assert(!loop.mcu.RawKeys);  // 松开首样本撤销原始按键，不等方向回中。
  loop.sample(4, 4095);
  input = loop.exchange();
  assert(input.flags == 3 && input.axis[0] == 1000 && loop.mcu.Authorized);
  std::cout << "PASS: delayed ENABLED_IDLE + real MCU authorization/deadman/release\n";
}

void confirmationFailure() {
  abr::JogEnableConfirmation confirmation;
  confirmation.begin(1, 1.0);
  assert(confirmation.update(1.01, 2, true, false) == abr::JogEnableConfirmation::Failed);
  assert(confirmation.update(1.02, 3, true, true) == abr::JogEnableConfirmation::Waiting);
  assert(!confirmation.pending());
  confirmation.begin(4, 2.0);
  assert(confirmation.update(3.0, 5, true, true) == abr::JogEnableConfirmation::Failed);
  confirmation.begin(6, 4.0); confirmation.cancel();
  assert(confirmation.update(4.1, 7, true, true) == abr::JogEnableConfirmation::Waiting);
  assert(!confirmation.pending());
  std::cout << "PASS: input loss, confirmation timeout, cancel, late status cannot rearm\n";
}

void fastResetAndCalibration() {
  Loopback loop;
  // MCU曾经使用请求5；PC轮询不中断，随后MCU复位并从0开始。
  loop.mcu.RequestId = 5; loop.mcu.Command = REMOTE_JOG_STOP;
  loop.sample(5); auto input = loop.exchange();
  assert(input.request_id == 5 && input.session == 100);
  Jog_Init(&loop.mcu);
  loop.now += 25;
  input = loop.exchange();
  assert(input.flags == 0 && input.request_id == 0 && input.session == 100);
  loop.now += 25;
  input = loop.exchange();
  assert(input.session == 101 && loop.poller.session() == 101);
  for (unsigned i = 0; i < 9; ++i) {
    loop.sample(5); input = loop.exchange();
    assert(input.session == 101);  // 校准期间不会每帧重复换session。
  }
  assert(loop.mcu.Ready && input.flags == 3 && input.request_id == 0);
  loop.sample(1);
  assert(Jog_Request(&loop.mcu, REMOTE_JOG_ENABLE, loop.now));
  loop.sample(5); input = loop.exchange();
  assert(input.request_id == 1 && input.session == 101);
  std::cout << "PASS: real MCU fast reset rotates session, calibrates, restarts request at 1\n";
}

int main() {
  delayedStateAndRealFirmware(); confirmationFailure(); fastResetAndCalibration();
}
