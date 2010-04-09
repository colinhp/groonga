/* -*- c-basic-offset: 2; coding: utf-8 -*- */
/*
  Copyright (C) 2010  Kouhei Sutou <kou@clear-code.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "../lib/grn-assertions.h"

#define get(name)                               \
  grn_ctx_get(context, name, strlen(name))
#define send_command(command)                   \
  cut_trace(grn_test_send_command(context, command))

void test_with_japanese_parenthesis(void);

static gchar *tmp_directory;
static gchar *database_path;

static grn_logger_info *logger;
static grn_ctx *context;
static grn_obj *database, *comments, *content, *expression, *variable, *result;

void
cut_startup(void)
{
  tmp_directory = g_build_filename(grn_test_get_base_dir(),
                                   "tmp",
                                   "test-table-select-normalize",
                                   NULL);
  database_path = g_build_filename(tmp_directory, "database.groonga", NULL);
}

void
cut_shutdown(void)
{
  g_free(database_path);
  g_free(tmp_directory);
}

static void
reset_variables(void)
{
  context = NULL;
  database = NULL;
  comments = NULL;
  content = NULL;
  expression = NULL;
  variable = NULL;
  result = NULL;
}

static void
remove_tmp_directory(void)
{
  cut_remove_path(tmp_directory, NULL);
}

static void
setup_ddl(void)
{
  send_command("table_create Comments TABLE_HASH_KEY ShortText");
  comments = get("Comments");
  send_command("column_create Comments content COLUMN_SCALAR ShortText");
  content = get("Comments.content");

  send_command("table_create Terms "
               "--flags TABLE_PAT_KEY|KEY_NORMALIZE "
               "--key_type ShortText "
               "--default_tokenizer TokenBigram");
  send_command("column_create Terms comment_content COLUMN_INDEX Comments "
               "--source content");
}

static void
setup_data(void)
{
  send_command("load "
               "'[[\"_key\",\"content\"],"
               "[\"ボロ\",\"うちのボロTV（アナログ...）はまだ現役です\"]]' "
               "Comments");
}

static void
setup_database(void)
{
  remove_tmp_directory();
  g_mkdir_with_parents(tmp_directory, 0700);

  database = grn_db_create(context, database_path, NULL);
  grn_test_assert_context(context);

  setup_ddl();
  setup_data();
}

void
cut_setup(void)
{
  reset_variables();

  logger = setup_grn_logger();

  context = g_new(grn_ctx, 1);
  grn_ctx_init(context, 0);

  setup_database();

  result = grn_table_create(context, NULL, 0, NULL,
                            GRN_TABLE_HASH_KEY | GRN_OBJ_WITH_SUBREC,
                            comments, NULL);
  grn_test_assert_context(context);
}

static void
teardown_database(void)
{
  if (variable) {
    grn_obj_unlink(context, variable);
  }

  if (expression) {
    grn_obj_unlink(context, expression);
  }

  if (result) {
    grn_obj_unlink(context, result);
  }

  if (content) {
    grn_obj_unlink(context, content);
  }

  if (comments) {
    grn_obj_unlink(context, comments);
  }

  if (database) {
    grn_obj_unlink(context, database);
  }
}

void
cut_teardown(void)
{
  teardown_database();

  if (context) {
    grn_ctx_fin(context);
    g_free(context);
  }

  teardown_grn_logger(logger);

  remove_tmp_directory();
}

static grn_obj *
query(const gchar *string)
{
  GRN_EXPR_CREATE_FOR_QUERY(context, comments, expression, variable);
  grn_test_assert(grn_expr_parse(context, expression,
                                 string, strlen(string),
                                 content, GRN_OP_MATCH, GRN_OP_AND,
                                 GRN_EXPR_SYNTAX_QUERY |
                                 GRN_EXPR_ALLOW_PRAGMA |
                                 GRN_EXPR_ALLOW_COLUMN));
  grn_test_assert_context(context);
  return expression;
}

void
test_create(gconstpointer data)
{
  cut_assert_not_null(grn_table_select(context, comments,
                                       query("content:@）は"),
                                       result, GRN_OP_OR));
  grn_test_assert_select(context,
                         gcut_take_new_list_string("ボロ", NULL),
                         result,
                         "_key");
}
