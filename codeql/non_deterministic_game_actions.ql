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
    this.getQualifiedName() = "std::rand" or
    // random_device::operator()
    exists(MemberFunction m |
      m = this and
      m.getDeclaringType().getName() = "random_device" and
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
    (this.getName() = "Execute" or this.getName() = "Query") and
    exists(Class c |
      c = this.getDeclaringType() and
      (
        c.getName() = "GameAction" or
        c.getABaseClass+().getName() = "GameAction"
      )
    )
  }
}

/**
 * Functions that are considered "barriers" because they only affect local client state
 * or are part of the UI/Audio systems which are inherently non-deterministic.
 */
predicate isBarrier(Function f) {
  f.getQualifiedName().matches("%Audio::%") or
  f.getQualifiedName().matches("%Ui::%") or
  f.getName() = [
    "GetTitleMusicDescriptor", "ApplyStyle", "Load", "onClose", "onMouseUp",
    "onMouseDown", "onMouseEnter", "onMouseLeave", "onMouseMove", "onMouseWheel",
    "onDraw", "onUpdate", "onPrepareDraw", "onPeriodicUpdate", "ShowError",
    "PlayTitleMusic", "GameLoadOrQuitNoSavePrompt", "SetActiveScene", "TitleInitialise",
    "Resume", "ErrorOpen", "ResetObjects", "InvalidateByNumber", "CloseByNumber",
    "CloseByCondition", "CloseByClass", "onPrepareDraw", "onUpdate"
  ] or
  exists(Type t | t = f.(MemberFunction).getDeclaringType() |
    t.getName().matches("%Scene") or
    t.getName() = ["WindowManager", "ProgressWindow", "WindowBase", "Audio"]
  ) or
  // Exclude common UI/Local-only paths
  f.getFile().getRelativePath().matches("%openrct2-ui/%")
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
          not m.getDeclaringType().getName() = "GameAction"
        )
      ) and
      not isBarrier(callee)
    )
  }

  query predicate nodes(Function f) { any() }

  class Node = Function;
}

import CallGraphPath

from GameActionMethod source, NonDeterministicFunction sink
where CallGraphPath::edges*(source, sink)
select sink, source, sink, "Non-deterministic call to " + sink.getName() + " reachable from game action method " + source.getQualifiedName() + "."
