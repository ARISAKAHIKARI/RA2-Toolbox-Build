#!/usr/bin/env python3
"""Check production cursor visibility guards before its missing-frame watchdog.

This source-order regression catches the first unfocused/client-loss tick,
before leaveGame has set menuSuspend. It does not simulate Win32 cursor output.
"""
from pathlib import Path
import unittest


def block(source, signature):
    start = source.index(signature)
    # Aggregate arguments such as contains({x,y}) are part of the condition,
    # not the opening of its guarded body.
    opening = source.index("(", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "(") - (source[end] == ")")
        end += 1
    opening = source.index("{", end)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return start, end, source[start:end]


class CursorVisibilityOrder(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(__file__).resolve().parents[2] / "cursor/source/main.cpp"
        source = path.read_text(encoding="utf-8")
        cls.draw = block(source, "static void draw(){")[2]
        cls.leave = block(source, "static void leaveGame(){")[2]
        cls.watch_start, _, cls.watch = block(
            cls.draw,
            'if(nativeGameRefresh&&!noCursorMode&&mode==L"hide"&&shared&&!nativeMenuHold&&!shared->menuSuspend){',
        )

    def guard_precedes_watchdog(self, signature):
        _, end, code = block(self.draw, signature)
        self.assertLess(end, self.watch_start)
        self.assertIn("return;", code)
        return code

    def test_first_unfocused_tick_suspends_before_timeout(self):
        code = self.guard_precedes_watchdog("if(!compatibleForeground){")
        self.assertIn("leaveGame();", code)
        self.assertIn("shared->menuSuspend=2;", self.leave)
        self.assertIn("nativeInvisibleAt=0;", self.leave)

    def test_hidden_minimized_invalid_or_outside_client_precedes_timeout(self):
        code = self.guard_precedes_watchdog("if(!IsWindow(game)||!IsWindowVisible(game)||IsIconic(game)||")
        for check in ("!pointValid", "!clientValid", "!noCursorMode&&!client.contains({pos.x,pos.y})"):
            self.assertIn(check, code)
        self.assertIn("leaveGame();", code)

    def test_unavailable_coordinate_window_is_recoverable_before_timeout(self):
        code = self.guard_precedes_watchdog(
            'if(!(nativeGameRefresh&&mode==L"hide")&&!noCursorMode&&!ensureCoordinateWindows(coordinates)){'
        )
        self.assertIn("leaveGame();", code)
        self.assertNotIn("autoPaused=", code)
        self.assertNotIn("stop();", code)

    def test_foreground_sibling_window_precedes_timeout(self):
        self.assertIn("leaveGame();", self.guard_precedes_watchdog("if(fp==pid&&fg!=game&&"))

    def test_resume_clears_old_deadline_before_watchdog(self):
        _, end, resume = block(self.draw, "if(!wasInside){")
        self.assertLess(end, self.watch_start)
        self.assertIn("if(shared&&shared->menuSuspend==2){shared->menuSuspend=0;nativeInvisibleAt=0;gameinput::wake(hook);}", resume)

    def test_exit_and_menu_handling_remain_before_timeout(self):
        self.assertIn("stop();", self.guard_precedes_watchdog("if(!live()){"))
        self.assertIn("enterNativeMenu();", self.guard_precedes_watchdog("if(gameMenuDialogActive()){"))

    def test_real_foreground_failure_still_times_out(self):
        self.assertEqual(self.draw.count("GetTickCount()-nativeInvisibleAt)>2000"), 1)
        self.assertIn("if(shared->nativeVisible)nativeInvisibleAt=0;", self.watch)
        self.assertIn("else if(!nativeInvisibleAt)nativeInvisibleAt=GetTickCount();", self.watch)
        self.assertIn("else if(uint32_t(GetTickCount()-nativeInvisibleAt)>2000)", self.watch)
        self.assertIn("autoPaused=true;stop();", self.watch)
        self.assertIn("return;", self.watch)


if __name__ == "__main__":
    unittest.main()
