<?php
module_load_include('inc', 'librusec', 'db');
module_load_include('inc', 'librusec', 'cache');

function lib404() {
  $host = $_SERVER['HTTP_HOST'];
  $ask = $_SERVER['REQUEST_URI'];
  if (preg_match('|^/index.php.q=(.+)|', $ask, $m)) $ask = $m[1];
  $url = parse_url($ask);
  $path = $url['path'] ?? '';
  $path = urldecode($path);
  if ($path[0] == '/') $path = substr($path, 1);
  $fixed = ['24' => 'stat/24', 'w' => '/stat/w', 'rss' => 'new/ru/rss'];
  $q = ''; 
  global $formats;
  if (strlen($path) >= 2) {
    $a0 = explode('/', $path)[0];
    if (isset($fixed[$path])) $q = $fixed[$path];
    elseif (is_lang($a0) || isset($formats[$a0])) $q = 'book/'.$path;
    elseif ($g = libgen($a0, 'code')) $q = 'g/'.$path; 
    elseif ($a = DB('avtors', 'aname')->aid("name=? ORDER BY books DESC", $a0)) $q = 'a/'.$a;
    elseif ($b = DB('book', 'N')->bid("Title=? AND NOT Deleted ORDER BY N DESC", $a0)) $q = 'b/'.$b;
    elseif ($s = DB('seqs')->sid("seqname=?", $a0)) $q = 's/'.$s;
    elseif ($m = DB('mags')->mid("title=?", $a0)) $q = 'm/'.$m;
    elseif (preg_match('|src/.+/\d+/(\d+)\.|', $path, $m)) $q = "b/$m[1]"; 
    elseif (preg_match('/(\d+)\.html$/', $path, $m)) $q = "a/$m[1]";
    elseif (preg_match('|pl/g.pl\?(.+)|', $path, $m)) $q = "g/$m[1]";
    elseif (preg_match('|search.pl\?ask=(.+)|', $path, $m)) $q = "search/$m[1]";
    elseif (preg_match('|search.pl\??(.*)|', $path, $m)) $q = "search/$m[1]";
	elseif (strlen($a0) == 32 && ctype_alnum($a0)) $q = 'b/'.$a0; // похоже на md5
  }
  if ($q && $q != $path) {   
    $_GET['q'] = $q;    
    menu_execute_active_handler();
	drupal_exit();
  } 
  drupal_set_title("Страница '$path' совсем не обнаружена");
  watchdog('librusec', "404 $ask");
  return "Ссылка, по которой вы пришли, не существует. Видимо, переименовалась. Это бывает.
Если, на ваш взгляд, ссылка правильная и не сработала по ошибке - сообщите об этом, пожалуйста, администрации сайта.
<br><br>
Если вы ищете книжку, воспользуйтесь поиском книг <a href=/b>http://$host/b</a> или авторов <a href=/a>http://$host/a</a>
<br><br>
Если вы ищете что-то другое, то, вероятно, вы ошиблись сайтом. Впрочем, может вам и попадется что интересное.  
Начинайте с главной страницы - <a href=/>http://$host/</a>. Удачи.";
}
