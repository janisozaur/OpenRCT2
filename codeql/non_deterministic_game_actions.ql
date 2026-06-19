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
    this.getQualifiedName().matches("%::rand") or
    // Catch C++ standard library random number generators
    exists(MemberFunction m |
      m = this and
      m.getDeclaringType().getName().matches("%random_device%") and
      m.getName() = "operator()"
    ) or
    exists(MemberFunction m |
      m = this and
      m.getDeclaringType().getName().matches("%mersenne_twister_engine%") and
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
      (c.getName() = "GameAction" or c.getABaseClass+().getName() = "GameAction")
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
    "GetTitleMusicDescriptor", "PlayTitleMusic", "Load", "onClose",
    "GameLoadOrQuitNoSavePrompt", "SetActiveScene", "TitleInitialise",
    "ShowError", "ErrorOpen", "ResetObjects", "InvalidateByNumber",
    "CloseByNumber", "CloseByCondition", "CloseByClass", "onPrepareDraw", "onUpdate"
  ] or
  // Exclude everything in openrct2-ui directory
  f.getFile().getRelativePath().matches("%openrct2-ui/%")
}

/**
 * Predicate representing an edge in the call graph.
 */
predicate callEdge(Function a, Function b) {
  exists(Call c |
    c.getEnclosingFunction() = a and
    (
      b = c.getTarget() or
      b = c.getTarget().(MemberFunction).getAnOverriddenFunction+()
    ) and
    // Block virtual dispatch noise through the base GameAction class.
    // This prevents jumping between unrelated actions via central dispatchers.
    not (
      c.getTarget().getName() = ["Execute", "Query"] and
      c.getTarget().(MemberFunction).getDeclaringType().getName() = "GameAction"
    )
  )
}

/**
 * Module required for path-problem queries.
 */
module GameActionPathGraph {
  /**
   * Predicate for edges in the path.
   */
  query predicate edges(Function a, Function b) {
    callEdge(a, b) and
    not isBarrier(a) and
    not isBarrier(b)
  }

  /**
   * Predicate for nodes in the path.
   */
  query predicate nodes(Function f) { any() }
}

import GameActionPathGraph

from GameActionMethod source, NonDeterministicFunction sink
where edges*(source, sink)
select sink, source, sink, "Non-deterministic call to " + sink.getName() + " reachable from " + source.getQualifiedName() + "."
