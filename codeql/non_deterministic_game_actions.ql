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
    (
      this.getName() = "UtilRand" or
      this.getName() = "UtilRandNormalDistributed"
    ) or
    this.hasGlobalName("rand") or
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
    exists(MemberFunction base |
      base.getDeclaringType().hasQualifiedName("OpenRCT2::GameActions", "GameAction") and
      (base.getName() = "Execute" or base.getName() = "Query") and
      this.getAnOverriddenFunction*() = base
    )
  }
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
      (
        // Static/Direct call
        callee = call.getTarget()
        or
        // Virtual call: if we call a method, any of its overrides could be the actual callee.
        exists(MemberFunction m |
          m = call.getTarget() and
          callee.(MemberFunction).getAnOverriddenFunction+() = m
        )
      )
    )
  }

  class Node = Function;
}

import CallGraphPath

from GameActionMethod source, NonDeterministicFunction sink
where CallGraphPath::edges*(source, sink)
select sink, source, sink, "Non-deterministic call to " + sink.getName() + " reachable from game action method " + source.getQualifiedName() + "."
