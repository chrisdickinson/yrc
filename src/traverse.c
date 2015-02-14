#include "traverse.h"

static void _traverse(yrc_ast_node_t* node, yrc_visitor_t* visitor, yrc_ast_node_t* parent, yrc_rel rel) {
  visitor->enter(node, rel, parent, visitor);
  yrc_llist_iter_t iterator;
  yrc_ast_node_t* child;

  switch (node->kind) {
    case YRC_AST_NULL:
    case YRC_AST_LAST:
      UNREACHABLE();
    break;

    case YRC_AST_PROGRAM:
      iterator = yrc_llist_iter_start(node->data.as_program.body);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_BODY);
      }
    break;

    case YRC_AST_STMT_BLOCK:
      iterator = yrc_llist_iter_start(node->data.as_block.body);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_BODY);
      }
    break;

    case YRC_AST_STMT_EMPTY:
    break;

    case YRC_AST_STMT_EXPR:
      _traverse(node->data.as_exprstmt.expression,
                visitor,
                node,
                REL_EXPRESSION);
    break;

    case YRC_AST_EXPR_CONDITIONAL:
    case YRC_AST_STMT_IF:
      _traverse(node->data.as_if.test,
                visitor,
                node,
                REL_TEST);
      _traverse(node->data.as_if.consequent,
                visitor,
                node,
                REL_CONSEQUENT);
      if (node->data.as_if.alternate)
      _traverse(node->data.as_if.alternate,
                visitor,
                node,
                REL_ALTERNATE);
    break;

    case YRC_AST_STMT_LABEL:
      /* XXX: add when labeled statements make it in */
    break;

    case YRC_AST_STMT_WITH:
      /* XXX: add when with statements make it in */
    break;

    case YRC_AST_CLSE_CASE:
      if(node->data.as_case.test)
      _traverse(node->data.as_case.test,
                visitor,
                node,
                REL_TEST);

      iterator = yrc_llist_iter_start(node->data.as_case.consequent);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_CONSEQUENT);
      }
    break;

    case YRC_AST_STMT_SWITCH:
      _traverse(node->data.as_switch.discriminant,
                visitor,
                node,
                REL_DISCRIMINANT);
      iterator = yrc_llist_iter_start(node->data.as_switch.cases);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_CASES);
      }
    break;

    case YRC_AST_STMT_THROW:
    case YRC_AST_STMT_RETURN:
      if (node->data.as_return.argument) {
        _traverse(node->data.as_return.argument,
                  visitor,
                  node,
                  REL_ARGUMENT);
      }
    break;

    case YRC_AST_STMT_TRY:
      _traverse(node->data.as_try.block,
                visitor,
                node,
                REL_BLOCK);
      if (node->data.as_try.handler) {
        _traverse(node->data.as_try.handler,
                  visitor,
                  node,
                  REL_HANDLER);
      }
      if (node->data.as_try.finalizer) {
        _traverse(node->data.as_try.finalizer,
                  visitor,
                  node,
                  REL_FINALIZER);
      }
    break;

    case YRC_AST_CLSE_CATCH:
      _traverse(node->data.as_catch.param,
                visitor,
                node,
                REL_PARAM);

      _traverse(node->data.as_catch.body,
                visitor,
                node,
                REL_BODY);
    break;

    case YRC_AST_CLSE_VAR:
      _traverse(node->data.as_vardecl.id,
                visitor,
                node,
                REL_ID);
      if (node->data.as_vardecl.init)
      _traverse(node->data.as_vardecl.init,
                visitor,
                node,
                REL_INIT);
    break;

    case YRC_AST_STMT_DOWHILE:
    case YRC_AST_STMT_WHILE:
      _traverse(node->data.as_while.test,
                visitor,
                node,
                REL_TEST);
      _traverse(node->data.as_while.body,
                visitor,
                node,
                REL_BODY);
    break;

    case YRC_AST_STMT_FOR:
      if (node->data.as_for.init)
      _traverse(node->data.as_for.init,
                visitor,
                node,
                REL_INIT);
      if (node->data.as_for.test)
      _traverse(node->data.as_for.test,
                visitor,
                node,
                REL_TEST);
      if (node->data.as_for.update)
      _traverse(node->data.as_for.update,
                visitor,
                node,
                REL_UPDATE);
      _traverse(node->data.as_for.body,
                visitor,
                node,
                REL_BODY);
    break;

    case YRC_AST_STMT_FOROF:
    case YRC_AST_STMT_FORIN:
      _traverse(node->data.as_for_in.left,
                visitor,
                node,
                REL_LEFT);
      _traverse(node->data.as_for_in.right,
                visitor,
                node,
                REL_RIGHT);
      _traverse(node->data.as_for_in.body,
                visitor,
                node,
                REL_BODY);
    break;

    case YRC_AST_EXPR_LITERAL:
    case YRC_AST_STMT_BREAK:
    case YRC_AST_STMT_CONTINUE:
    case YRC_AST_EXPR_IDENTIFIER:
    case YRC_AST_EXPR_THIS:
    break;

    case YRC_AST_DECL_VAR:
      iterator = yrc_llist_iter_start(node->data.as_var.declarations);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_DECLARATIONS);
      }
    break;

    case YRC_AST_EXPR_ARRAY:
      if (node->data.as_array.elements) {
        iterator = yrc_llist_iter_start(node->data.as_array.elements);
        while ((child = yrc_llist_iter_next(&iterator))) {
          _traverse(child, visitor, node, REL_ELEMENTS);
        }
      }
    break;

    case YRC_AST_EXPR_OBJECT:
      if (node->data.as_object.properties) {
        iterator = yrc_llist_iter_start(node->data.as_object.properties);
        while ((child = yrc_llist_iter_next(&iterator))) {
          _traverse(child, visitor, node, REL_PROPERTIES);
        }
      }
    break;

    case YRC_AST_EXPR_PROPERTY:
      _traverse(node->data.as_property.key,
                visitor, node, REL_KEY);
      _traverse(node->data.as_property.expression,
                visitor, node, REL_EXPRESSION);
    break;

    case YRC_AST_DECL_FUNCTION:
    case YRC_AST_EXPR_FUNCTION:
      iterator = yrc_llist_iter_start(node->data.as_function.params);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_PARAMS);
      }
      iterator = yrc_llist_iter_start(node->data.as_function.defaults);
      while ((child = yrc_llist_iter_next(&iterator))) {
        if (child)
        _traverse(child, visitor, node, REL_DEFAULTS);
      }
      _traverse(node->data.as_function.body, visitor, node, REL_BODY);
    break;

    case YRC_AST_EXPR_ARROW:
      /* XXX: add when arrow expressions make it in */
    break;

    case YRC_AST_EXPR_SEQUENCE:
      child = node;
      do {
        _traverse(child->data.as_sequence.left, visitor, node, REL_EXPRESSION);
        child = node->data.as_sequence.right;
      } while(child->kind == YRC_AST_EXPR_SEQUENCE);
      _traverse(child, visitor, node, REL_EXPRESSION);
    break;

    case YRC_AST_EXPR_UNARY:
      _traverse(node->data.as_unary.argument, visitor, node, REL_ARGUMENT);
    break;

    case YRC_AST_EXPR_BINARY:
    case YRC_AST_EXPR_LOGICAL:
    case YRC_AST_EXPR_ASSIGNMENT:
      _traverse(node->data.as_binary.left, visitor, node, REL_LEFT);
      _traverse(node->data.as_binary.right, visitor, node, REL_RIGHT);
    break;

    case YRC_AST_EXPR_UPDATE:
      _traverse(node->data.as_unary.argument, visitor, node, REL_ARGUMENT);
    break;

    /* erk */
    case YRC_AST_EXPR_NEW:
    break;

    case YRC_AST_EXPR_CALL:
      _traverse(node->data.as_call.callee, visitor, node, REL_CALLEE);
      iterator = yrc_llist_iter_start(node->data.as_call.arguments);
      while ((child = yrc_llist_iter_next(&iterator))) {
        _traverse(child, visitor, node, REL_ARGUMENTS);
      }
    break;

    case YRC_AST_EXPR_MEMBER:
      _traverse(node->data.as_member.object, visitor, node, REL_OBJECT);
      _traverse(node->data.as_member.property, visitor, node, REL_PROPERTY);
    break;

    case YRC_AST_EXPR_YIELD:
    break;

  }
  visitor->exit(node, rel, parent, visitor);
}

static yrc_visitor_mode nop(yrc_ast_node_t* node, yrc_rel rel, yrc_ast_node_t* parent, void* ctx) {
  return kYrcTraverseContinue;
}

void yrc_traverse(yrc_ast_node_t* node, yrc_visitor_t* visitor) {
  if (visitor->enter == NULL) visitor->enter = nop;
  if (visitor->exit == NULL) visitor->exit = nop;
  _traverse(node, visitor, NULL, REL_NONE);
}
