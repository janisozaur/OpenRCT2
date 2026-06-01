/**
 * @name Call to non-deterministic random function from game action
 * @description GameActions must be deterministic across all clients. Calling non-deterministic
 *              random functions like UtilRand or UtilRandNormalDistributed from a GameAction's
 *              Execute or Query method (or any function they call) can lead to state
 *              desyncs in multiplayer.
 * @kind path-problem
 * @problem.severity error
 * @precision high
 * @id cpp/openrct2/non-deterministic-game-action
 * @tags reliability
 *       security
 *       external/openrct2
 */

import cpp

/**
 * A function that is considered non-deterministic for OpenRCT2 game state.
 * These functions use local entropy or unseeded PRNGs that differ between clients.
 */
class NonDeterministicFunction extends Function {
  NonDeterministicFunction() {
    this.getName() = ["UtilRand", "UtilRandNormalDistributed", "rand"] or
    this.hasQualifiedName("std", "rand") or
    // random_device::operator()
    exists(MemberFunction m |
      m = this and
      m.getDeclaringType().hasQualifiedName("std", "random_device") and
      m.getName() = "operator()"
    )
  }
}

/**
 * The Execute or Query methods of a GameAction.
 * These are the entry points for game state changes or queries that must be deterministic.
 */
class GameActionMethod extends MemberFunction {
  GameActionMethod() {
    this.getAnOverriddenFunction*().hasQualifiedName("OpenRCT2", "GameActions", "GameAction", ["Execute", "Query"])
  }
}

/**
 * Functions that are considered "barriers" because they only affect local client state
 * or are part of the UI/Audio systems which are inherently non-deterministic but
 * shouldn't be reached from deterministic game logic (if they are, it's usually
 * a side effect like closing a window).
 */
predicate isBarrier(Function f) {
  f.hasQualifiedName("OpenRCT2", "Audio", _, _) or
  f.hasQualifiedName("OpenRCT2", "Ui", _, _) or
  f.hasQualifiedName("OpenRCT2", "Audio", _) or
  f.hasQualifiedName("OpenRCT2", "Ui", _) or
  f.getName() = [
    "GetTitleMusicDescriptor", "ApplyStyle", "Load", "onClose", "onMouseUp",
    "onMouseDown", "onMouseEnter", "onMouseLeave", "onMouseMove", "onMouseWheel",
    "onDraw", "onUpdate", "onPrepareDraw", "onPeriodicUpdate"
  ] or
  exists(Type t | t = f.(MemberFunction).getDeclaringType() |
    t.getName() = ["TitleScene", "WindowManager", "ProgressWindow"]
  ) or
  // Exclude common UI/Local-only paths
  f.getFile().getRelativePath().matches("src/openrct2-ui/%")
}

/**
 * A module for tracing call paths.
 */
module CallGraphPath {
  /**
   * Predicate to follow the call graph, including virtual calls.
   */
  query predicate edges(Function caller, Function callee) {
    exists(Call call |
      call.getEnclosingFunction() = caller and
      not isBarrier(caller) and
      (
        // Static/Direct call
        callee = call.getTarget()
        or
        // Virtual call: follow overrides, but avoid the generic dispatch noise through GameAction base
        exists(MemberFunction m |
          m = call.getTarget() and
          callee.(MemberFunction).getAnOverriddenFunction+() = m and
          // Avoid noise from broad interfaces that would connect unrelated actions
          not m.hasQualifiedName("OpenRCT2", "GameActions", "GameAction", ["Execute", "Query"])
        )
      )
    )
  }

  query predicate nodes(Function f) { any() }

  class Node = Function;
}

import CallGraphPath

from GameActionMethod source, NonDeterministicFunction sink, Call call
where
  CallGraphPath::edges*(source, call.getEnclosingFunction()) and
  call.getTarget() = sink
select call, source, sink, "Non-deterministic call to " + sink.getName() + " reachable from game action method " + source.getQualifiedName() + "."
