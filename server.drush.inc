<?php
/**
 * @file Drush server commands
 */

/**
 * Implementation of hook_drush_help().
 */
function server_drush_help($section) {
  switch ($section) {
    case 'meta:server:title':
      return dt('Server commands');
    case 'meta:server:summary':
      return dt('Commands to help run a simple development server from your Drupal root.');
  }
}

/**
 * Implementation of hook_drush_command().
 */
function server_drush_command() {
  $items['server'] = array(
    'description' => 'Start a simple server from the command-line with your Drupal root as the document root.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION,
    'aliases' => array('s'),
  );
  return $items;
}

/**
 * Start server in the Drupal root with the version specific start script.
 */
function drush_server($arg = 'start') {
  if (!in_array($arg, array('start', 'stop', 'restart', 'graceful'))) {
    return drush_set_error('DRUSH_SERVER_INVALID_ARG', dt('Invalid argument.'));
  }

  if ($arg == 'start') {
    drush_print(dt('Starting server...'));
  }

  $root = drush_get_context('DRUSH_DRUPAL_ROOT');
  $conf = _drush_server_conf($root);
  _drush_server_create_conf_file($root);

  $command = join(' ', array(
    'apachectl',
    "-f \"{$conf['conf_file']}\"",
    "-k \"$arg\"",
  ));

  drush_log($command);
  $descriptor_spec = array(
    0 => STDIN,
    1 => array('pipe', 'w'),
    2 => array('pipe', 'w'),
  );
  $process = proc_open($command, $descriptor_spec, $pipes, $root);

  if (is_resource($process)) {
    $output = _drush_server_process_pipes($process, $pipes);
    proc_close($process);

    switch ($arg) {
      case 'start':
        drush_log(dt('Server started on !host:!port.', array('!host' => $conf['host'], '!port' => $conf['port'])), 'ok');
        drush_print(dt('    Config file:           !conf_file', array('!conf_file' => $conf['conf_file'])));
        drush_print(dt('    Error log:             !log_dir/error_log', array('!log_dir' => $conf['log_dir'])));
        drush_print(dt('    Access log:            !log_dir/access_log', array('!log_dir' => $conf['log_dir'])));
        break;
      case 'stop':
        drush_log(dt('Server stopped on !host:!port.', array('!host' => $conf['host'], '!port' => $conf['port'])), 'ok');
        break;
      case 'restart':
      case 'graceful':
        drush_log(dt('Server restarted on !host:!port.', array('!host' => $conf['host'], '!port' => $conf['port'])), 'ok');
        break;
    }
  }
}

function _drush_server_process_pipes($process, $pipes) {
  $output = FALSE;

  $status = proc_get_status($process);
  while ($status['running']) {
    $stdout = stream_get_contents($pipes[1]);
    if (!empty($stdout)) {
      drush_log(rtrim($stdout), 'warning');
      $output = TRUE;
    }
    $stderr = stream_get_contents($pipes[2]);
    if (!empty($stderr)) {
      drush_log(rtrim($stderr), 'error');
      $output = TRUE;
    }
    sleep(1);
    $status = proc_get_status($process);
  }
  fclose($pipes[1]);
  fclose($pipes[2]);

  return $output;
}

function _drush_server_conf($root) {
  static $conf = array();

  if (empty($conf)) {
    $conf['tmp_dir'] = drush_get_option(array('tmp-dir'), FALSE);
    if (empty($conf['tmp_dir'])) {
      // Try to use the alias if available.
      $alias_context = drush_get_context('alias');
      if (!empty($alias_context['name'])) {
        $conf['tmp_dir'] = drush_find_tmp() .'/'. str_replace('@', '', $alias_context['name']);
      }
      // Last resort.  Let drush create a temp dir.
      else {
        $conf['tmp_dir'] = drush_tempdir();
      }
    }
    $conf['host'] = drush_get_option(array('host', 'h'), 'localhost');
    $conf['port'] = drush_get_option(array('port', 'p'), 3000);
    $conf['log_dir'] = drush_get_option(array('log-dir'), $conf['tmp_dir'] .'/log');

    $conf['doc_root'] = $root;
    $conf['conf_file'] = $conf['tmp_dir'] .'/httpd.conf';

    if (!drush_mkdir($conf['tmp_dir'])) {
      return drush_set_error('DRUSH_SERVER_NO_TMP_DIR', dt('Unable to create temporary directory !dir.', array('dir' => $conf['tmp_dir'])));
    }

    if (!drush_mkdir($conf['log_dir'])) {
      return drush_set_error('DRUSH_SERVER_NO_LOG_DIR', dt('Unable to create log directory !dir.', array('dir' => $conf['log_dir'])));
    }
  }

  return $conf;
}

function _drush_server_create_conf_file($root) {
  $conf = _drush_server_conf($root);

  $commands = drush_get_commands();
  $template = $commands['server']['path'] .'/includes/httpd_osx.conf';

  $directives[] = "#";
  $directives[] = "# Base configuration from $conf[conf_file]";
  $directives[] = "#";
  $directives[] = file_get_contents($template);

  $directives[] = "\n\n";

  $directives[] = "#";
  $directives[] = "# Custom configuration built by drush";
  $directives[] = "#";
  $directives[] = "PidFile $conf[tmp_dir]/httpd.pid";
  $directives[] = "LockFile $conf[tmp_dir]/accept.lock";
  $directives[] = '';
  $directives[] = "ServerName $conf[host]";
  $directives[] = "Listen $conf[port]";
  $directives[] = '';
  $directives[] = "ErrorLog $conf[log_dir]/error_log";
  $directives[] = 'LogFormat "%h %l %u %t \"%r\" %>s %b" common';
  $directives[] = "CustomLog $conf[log_dir]/access_log common";
  $directives[] = '';
  $directives[] = "DocumentRoot \"$conf[doc_root]\"";
  $directives[] = "<Directory \"$conf[doc_root]\">";
  $directives[] = "  AllowOverride all";
  $directives[] = "</Directory>";

  $content = join("\n", $directives);
  file_put_contents($conf['conf_file'], $content);

  return $conf['conf_file'];
}
