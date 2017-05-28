<?php
module_load_include('inc', 'librusec', 'db');

function lib404() {
  $host = $_SERVER['HTTP_HOST'];
  $ask = $_SERVER['REQUEST_URI'];
  if (preg_match('|^/index.php.q=(.+)|', $ask, $m)) $ask = $m[1];
  $u = parse_url($ask);
  $path = $u['path'] ?? '';
  $path = urldecode($path);
  if ($path[0] == '/') $path = substr($path, 1);
  $fixed = ['24' => 'stat/24', 'w' => '/stat/w', 'rss' => 'new/ru/rss'];
  $q = '';
  if (strlen($path) >= 2) {
    if (isset($fixed[$path])) $q = $fixed[$path];
    elseif (strlen($path) < 5 && ctype_alnum($path) && (DB('book')->bid("FileType=? OR Lang=?", $path, $path))) $q = 'book/'.$path;
    elseif ($g = DB('genres')->gid("code=?", $path)) $q = 'g/'.$g; 
    elseif ($a = DB('avtors', 'aname')->aid("name=? ORDER BY books DESC", $path)) $q = 'a/'.$a;
    elseif ($b = DB('book', 'N')->bid("Title=? AND NOT Deleted ORDER BY N DESC", $path)) $q = 'b/'.$b;
    elseif ($s = DB('seqs')->sid("seqname=?", $path)) $q = 's/'.$s;
    elseif ($m = DB('mags')->mid("title=?", $path)) $q = 'm/'.$m;
    elseif (preg_match('|src/.+/\d+/(\d+)\.|', $path, $m)) $q = "b/$m[1]"; 
    elseif (preg_match('/(\d+)\.html$/', $path, $m)) $q = "a/$m[1]";
    elseif (preg_match('|pl/g.pl\?(.+)|', $path, $m)) $q = "g/$m[1]";
    elseif (preg_match('|search.pl\?ask=(.+)|', $path, $m)) $q = "search/$m[1]";
    elseif (preg_match('|search.pl\??(.*)|', $path, $m)) $q = "search/$m[1]";
  }
  if ($q && $q != $path) {   
    $_GET['q'] = $q;    
    menu_execute_active_handler();
	exit();
  } else {
    drupal_set_title("Страница '$path' совсем не обнаружена");
    watchdog('librusec', "404 $ask");
    return "Ссылка, по которой вы пришли, не существует. Видимо, переименовалась. Это бывает.
  Если, на ваш взгляд, ссылка правильная и не сработала по ошибке - сообщите об этом, пожалуйста, администрации сайта.
<br><br>
Если вы ищете книжку, воспользуйтесь поиском книг <a href=/b>http://$host/b</a> или авторов <a href=/a>http://$host/a</a>
<br><br>
Если вы ищете что-то другое, то, вероятно, вы ошиблись сайтом. Впрочем, может вам и попадется что интересное.  Начинайте с главной страницы - <a href=/>http://$host/</a>. Удачи.";
 }
}
