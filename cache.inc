<?php
module_load_include('inc', 'memcache');
module_load_include('inc', 'librusec', 'db');

/**
 * @file
 * Caching functions from the librusec module. Based on APC and Mysql MEMORY table
**/

/******************************************************************************************
* Система кэширования с элементами неестественного интеллекта
* При повышенной загрузке отдаёт старьё, при пониженной - перегенерирует при необходимости.
* На время генерации может хапнуть семафор, чтоб свежепротухшая популярная страничка 
* не генерировалась десятком клиентов одновременно
*******************************************************************************************/

// Родной друпальный кэш слишком наворочен, но при этом, кажется, не умеет тэгов и деления на быстрый/медленный.
// до конца познать его хорактер мне не удалось.
// пришлось велосипедить.

// APC кэш. 
// + работает быстрее любого другого способа 
// - работает внутри пыхсервера, недоступен с других серверов, 
// -+ обнуляется при перезагрузке php

// взять из APC. 
// ID образуется автоматически из имени вызывающей функции + $id опционально
// и сохраняется для последующего вызова setapc
// нормальный сценарий: if ($r = getapc()) return $r; else return setpac(AnySlowFunction());
// по умолчанию сброс через 55 секунд
// возвращает результат или пустую строку
function getapc($id = '', $tm = 155) {
  Global $apc_cid, $apc_cache;
  $f = debug_backtrace(1);
  $f = $f[1];
  $ids[] = $f['function'];
  $ids[] = !uid();
  if ($id == '') {
    if (isset($f['object'])) $ids[] = $f['object'];
    if (isset($f['args'])) $ids[] = $f['args']; 
  } else {
    $ids[] = $id;
  }
  $cid = $apc_cid[$f['function']] = json_encode($ids);
  if (isset($apc_cache[$cid])) return $apc_cache[$cid];
  if ($r = apc_fetch($cid)) { 
    $r = json_decode($r);
    if ($f == 'LA' || $tm == 0 || REQUEST_TIME - $r[0] < $tm*(uid() ? 1 : 10) || ($la = LA()) > 1 || !uid() && $la > 0.5) return $apc_cache[$cid] = $r[1];
    apc_delete($cid);
  }   
  return NULL;
}

// положить $val в APC на $tm секунд (0 - хранить до смерти php). 
// ID сохраняется с предыдущего вызова getapc, который всегда идёт перед setapc.
// возвращает $val для типичного окончания одной строкой return setapc(...); вместо трёх $r = ...; setapc($r); return $r;
function setapc($val, $tm = 0) {
  Global $apc_cid, $apc_cache;
  $f = debug_backtrace(1);
  $f = $f[1]['function'];
  if ($cid = $apc_cid[$f]) {
    apc_store($cid, json_encode([REQUEST_TIME, $val]), $tm);
    $apc_cache[$cid] = $val;
  } else {
    stop();
  }  
  return $val;
}

// mysql MEMORY table
// - заметно тормознее APC
// + один общий для всех пыхсерверов, переживает перезапуск php

function now() {
  static $now = '';
  if (!$now) $now = db_query('SELECT NOW()')->fetchField(); 
  return $now;	
}

// попытка прочитать из кэша
// при маленькой загрузки проверяется свежесть результата, при большой любой сойдёт
$memcache = 0;
function _libcg($cid, $t) {
  Global $memcache;	
  if (!$memcache) {
    $memcache = 1;
    if (class_exists('Memcached')) {
      $memcache = new Memcached();
      if (!$memcache->addServer("127.0.0.1", 11211))
	    $memcache = 1;
	}	
  }
  if (!is_object($memcache)) { // не получилось с мемкэшем, кэшируем в базу
	$W = DB('cache')->hsr('time,value,raw,tag', $cid, 1);
//    $W = DB('cache')->select('time,value,raw,tag', "cid=? LIMIT 1", $cid);
  } else {
    $W = $memcache->get($cid);	  
  }	
  if (!$W) return null;
  if ($t && $W->tag != 'perennially' && LA() < 1) {
    $delay = strtotime(now()) - strtotime($W->time);    
    if ($delay > $t) return null;
  }
  apc_inc('cache_hit');
  if ($W->raw || !$W->value) return $W->raw;
  if (($r = json_decode($W->value)) === NULL) {
	liblog("Unserialize error on $cid");
	libcd($cid);
	return null;
  }	
  return $r;
}

// libCacheGet
// чтение из кэша, с опциональной проверкой и захватом семафора 
// при отсутствии возвращает null, при наличии пустой строки ' ';
function libcg($cid, $t = 7500, $lock = 0) {
  Global $wait_lock_id;
  apc_inc('cache_read');
  $cid = trim($cid);
  if (!$cid) return null;
  if (strlen($cid) > 32 || !mb_check_encoding($cid, 'ASCII')) $cid = md5($cid);  
  if ($r = _libcg($cid, $t)) return $r;
  if ($lock) {
    if (lock_acquire($cid)) {
      $wait_lock_id[$cid] = 1;
    } else {  
      lock_wait($cid);
      if ($r = _libcg($cid, $t)) return $r;
      if (lock_acquire($cid)) $wait_lock_id[$cid] = 1;
    } 
  }
  return null;  
}

//libCacheSet
function libcs($cid, $data, $tag = '') {
  Global $wait_lock_id, $memcache;
  $cid = trim($cid);
  if (!$cid) return $data;

  if (strlen($cid) > 32 || !mb_check_encoding($cid, 'ASCII')) $cid = md5($cid);
  if (is_string($data)) {
    if (strlen($data) > 6666) return $data;
	$raw = $data;
    $val = '';	
  } else {	  
    $val = json_encode($data);
    if (strlen($val) > 6666) return $data;
	$raw = '';
  }	

  if (!$memcache) {
    $memcache = 1;
    if (class_exists('Memcached')) {
      $memcache = new Memcached();
      if (!$memcache->addServer("127.0.0.1", 11211))
	    $memcache = 1;
	}	
  }
  if (is_int($memcache)) { // не получилось с мемкэшем, кэшируем в базу
    try {
      db_query("INSERT INTO libcache(cid, tag, value, raw) VALUES (?,?,?,?) ON DUPLICATE KEY UPDATE value=?, raw=?, time=NOW()", [$cid, $tag, $val, $raw, $val, $raw]);
    } catch(PDOException $e) {
  	  liblog($e, $cid, $tag, $val);  
    }	  
  } else {
	$memcache->set($cid, (object) ['tag' => $tag, 'value' => $val, 'raw' => $raw, 'time' => now()]);	  
  }	

  if ($wait_lock_id[$cid] ?? 0) lock_release($cid);

  if (!apc_fetch('cache_store')) {
    apc_store('cache_store', 1);	
    apc_store('cache_read', 0);
    apc_store('cache_hit', 0);
  } else {
    apc_inc('cache_store');
  }  
  return $data;
}

// Удаляем из кэша всё связанное с даннами тэгами
function libsettag($tag1, $tag2 = NULL) { 
  if ($tag1) DB('cache')->delete("tag=?", $tag1);
  if ($tag2) DB('cache')->delete("tag=?", $tag2);
}

//libCacheDelete
function libcd($cid) {
  if ($cid) DB('cache')->delete("cid=?", $cid);
}

// кешированные описания жанров. по id, коду или названию возвращает объект или запрошенное поле.
function libgen($g, $col = '*') {
  static $genres;
  if (!$g) return 0;
  if (!isset($genres[$g])) {
    if (!apc_fetch('libcachegen')) {
      if(!DB('information_schema.TABLES')->table_rows("TABLE_NAME LIKE 'libcachegen'")) { 			
		db_query("INSERT INTO libcachegen SELECT gid, gid as name FROM libgenres");
		db_query("INSERT INTO libcachegen SELECT gid, code as name FROM libgenres");
		db_query("INSERT INTO libcachegen SELECT gid, edesc as name FROM libgenres WHERE edesc NOT LIKE ''");
		db_query("INSERT INTO libcachegen SELECT gid, gdesc as name FROM libgenres WHERE gdesc NOT LIKE ''");
      }			
	  apc_store('libcachegen', 1);
	}  
	if (($gid = DB('cachegen')->hsr('gid', $g, 1, 'name')) && ($a1 = DB('genres')->row($gid))) 
      $genres[$g] = $genres[$a1->gid] = $genres[$a1->code] = $genres[$a1->gdesc] = $genres[$a1->edesc] = $a1;
    else $genres[$g] = ''; 
  }
  if ($genres[$g] == '') return 0;
  return $col == '*' ? $genres[$g] : $genres[$g]->$col;
}

// ссылка на юзера по uid
function ul($u, $style = '') {
  if (!$u) return '';
  $r = check_plain(uname($u));
  if ($style == 'nolink' || !uid()) return $r;
  if ($style == 'marknew') {
    static $created;	  
	if (!isset($created[$u])) $created[$u] = DB('users')->created($u);
    if (REQUEST_TIME - $created[$u] < 300*24*3600) return "<a href=/user/$u><font color=Coral>$r</font></a>";
  }	  
  return "<a href=/user/$u>$r</a>";
}

//cсылка на ноду
function nl($nid) {
  if (!$r = getapc($u, 0)) $r = setapc(DB('node')->title($nid));
  $r = check_plain($r);
  return "<a href=/node/$nid>$r</a>";
}

// ccылка на автора по uid
// $t разделитель
// $o опции
function avl($a, $tt=' ', $o='', $ltlink = '', $an = '') {  
  if (is_lang($tt)) $t = ' ';
  else $t = $tt;
  if ($t == '') $t = ' '; 
  if (is_array($a)) {
    foreach($a as $aid) {
      if ($r) $r .= "$t-$t";
      $r .= avl($aid, $tt, $o, $ltlink);
    }
    return $r;     
  }
  $cnt = '';  
  if ($o == 'cnt') {
    if ($cnt = DB('avtors')->books($a)) $cnt = "($cnt)";
    $o = ''; 
  }   
  $an = $an ? : AvtorName($a, $tt, $o);
  if ($t == '&nbsp;') $an = str_replace(' ', $t, $an);
  if (mb_strlen($an) < 3) $an .= ' &nbsp ';
  $r = "<a href=/a/$a$ltlink class=\"colorlnk\">$an</a>$cnt";
  if (user_access('библиотекарь') || user_has_role(6) || uid() < 1000) {
	static $metki = 1;
    if ($metki == 1) 
	  $metki = DB('metka')->idS("uid=?",uid());	
	if (in_array('a'.$a, $metki))
      $r = "<img src=/img/ok.png>".$r;
  } 
  return $r;	   
}

// выдаёт название книги (с ссылкой) на bid
// $style: size nolink rand read/rand edit
function bl($b, $style='', $len=0) {  
  if (!$b) return '';
  static $bb, $bo;
  if (isset($b->bid)) {
    $a1 = $b;
    $b = $a1->bid;
  }
  if (!isset($a1->SrcLang)) {
	if (isset($bb[$b])) 
	  $a1 = $bb[$b];
    elseif ($a1 = DB('book')->hsr("Title,Title1,FileSize,Lang,Deleted,FileType,SrcLang,Pages", $b)) 
	  $bb[$b] = $a1;
	else
	  return '';
  }
  $origin = ' ';  
  if ($style != 'noorig') { 
    if (isset($bo[$b])) {
	  $origin = $bo[$b];
    } else {
      if ($a1->SrcLang != $a1->Lang)
        if ($or = DB('rel')->bid2("bid1=$b AND relation='t'")) 
          $origin = " [<a href=/b/$or>".libTitle('b', $or)."</a> $a1->Lang]";
	}
    $bo[$b] = $origin;	
  }
  $t = $a1->Title;
  if ($a1->Title1) $t .= " [$a1->Title1]";
  if ($len > 2) if (mb_strlen($t) > $len) $t = mb_substr($t, 0, $len-2).'...';
  if ($style != 'nolink') $t .= "</a>";
  if ($origin && $origin != ' ') $t .= $origin;
  elseif (isset($a1->Lang) && $a1->Lang && $a1->Lang != 'ru') $t .= " [$a1->Lang]";
	  
  if ($style == 'size' || $style == 'nolink' || $style == 'type' || $style == 'origin') {
    if ($a1->Deleted != 2) {
      $size = intval($a1->FileSize/1024);
      if ($size < 2345) $size .= 'K';  
      else $size = intval($size/1024).'M';
	  if ($a1->Pages) $size .= ", $a1->Pages с.";
      $t .= " <span class=size>$size</span>";    
    }  
    if ($style == 'nolink') return $t;
  }  
  if ($style == 'type' && $a1->Deleted != 2) $t .= ' '.$a1->FileType;
  if ($style && $style[0] == 'r' || $style == 'edit') $b .= '/'.$style;
  $r = "<a href=/b/$b class=\"colorlnk\">$t"; 
  if ($a1->Deleted == 2) $r .= " (отсутствует)";
  return $r;
}

function libaid($b) {
  if ($r = getapc('', 1600)) return $r;
  return setapc(DB('avtor')->aid("bid=$b ORDER BY role"));
}

function libAvtorsNames($b, $limit = 99, $link = 1, $lang = 'ru') {
  if ($r = getapc()) return $r;
  foreach(DB('avtor')->select('aid', "bid=? AND role='a' LIMIT ".(1+$limit), $b) as $a) {
    if ($r) $r .= " &nbsp; ";
    if ($lang != 'ru') if ($a2 = DB('avtors')->aid("main=$a AND lang IN('$lang','')")) $a = $a2;
    $r .= $link ? avl($a, '&nbsp;') : AvtorName($a);
    if (--$limit < 0) return setapc('Сборник');
  }
  return setapc($r);
}

function libAvtors($b, $role = 'a', $limit = 1, $cache = 1) {
  static $aaa;
  if (!isset($aaa[$b])) $aaa[$b] = DB('avtor')->chsr(300, 'aid,role', $b, 100);
  $r = []; $i = 0;
  foreach($aaa[$b] as $a) {
    if ($role == '*' || $role == $a->role) {
      if ($limit == 1) return $a->aid;
      $r[] = $a->aid; 
      if ($i++ > $limit) break;
    }  
  }
  return $r;
}

function AvtorNames($a, $reset = 0) {
  if (!$a1 = DB('avtors')->hsr('FirstName,MiddleName,LastName,NickName,pna,pnb,pnc,pnd,pnf,png', $a)) return liblog("AVTORNAMES", $a);
  foreach(explode(',', 'FirstName,MiddleName,LastName,NickName') as $c) if (mb_strlen($a1->$c) == 1) $a1->$c .= '.';
  return $a1;
}

function AvtorName($a, $t=' ', $LastFirst = 0) {
  if (!$a = intval($a)) return '';
  $lang = $an = '';
  if (is_lang($t)) {
    if ($t != 'ru') $lang = $t;
    $t = ' ';
  }  
  if ($lang) {
    if ($aa = DB('avtors')->aid("main=$a AND lang='$lang'")) $a = $aa;
    elseif ($lang == 'de' AND ($aa = DB('avtors')->aid("main=$a AND lang='en'"))) $a = $aa;
    elseif ($lang == 'en' AND ($aa = DB('avtors')->aid("main=$a AND lang='de'"))) $a = $aa;
  }  
  if (!$a1 = AvtorNames($a)) return '';
  if ($t == 'lastname') {
    $an = check_plain($a1->LastName);
  } else {
    if ($a1->pnc) {
      if (!strlen($a1->pna)) $a1->pna = $a1->LastName;
      if ($a1->png) $an = check_plain($a1->png).$t;
      if ($LastFirst) {
        if ($an) $an = $t.$an; 
        $an = check_plain($a1->pna).$an;
      } else {
        $an .= check_plain($a1->pna);
      }
      $an .= ', '. check_plain($a1->pnc);
    } else {
      if ($a1->FirstName) $an = check_plain($a1->FirstName).$t;
      if ($a1->MiddleName) $an .= check_plain($a1->MiddleName).$t;
      if ($LastFirst) {
        if ($an) $an = $t.$an; 
        $an = check_plain($a1->LastName).$an;
      } else {
        $an .= check_plain($a1->LastName);
      }  
    }  
    if ($a1->NickName) 
      if ($an) $an .= "$t(".check_plain($a1->NickName).")"; 
      else $an = check_plain($a1->NickName);  
  }
  return $an;
}

function libListBlackGenre($reset = 0) {
  if (!$u = uid()) return '';
  static $gg = -1;
  if ($gg != -1) return $gg;
  $r = '';
  foreach(DB('userblackgenre')->select('gid', "uid=$u ORDER BY gid") as $g) {
    if (!$r) $r = 'Скрытые жанры: ';
    else $r .= " &nbsp; ";
    if (!$a1 = libgen($g)) stop($a1, $g);
    $r .= "<a href=/g/$a1->gid/show>$a1->gdesc</a>";
  }  
  if ($r) {
    $gg = "<div class=genre>$r</div>";
  } else {
    $gg = '';
  }
  return $gg;
}

function libListBlackGG(int $uid = 0) : string {
  if (!$uid) $uid = uid();
  if (!$uid) return ''; 
  static $gg = -1;
  if ($gg == -1) $gg = DB('userblackgenre')->select("GROUP_CONCAT(CAST(gid AS CHAR) ORDER BY gid)", "uid = ? LIMIT 1", $uid);
  return $gg;
}

function libKnown($a, $reset = 0) {
  if ($r = getapc($a, 0)) if (!$reset) return $r == 1 ? '' : $r;
  $a1 = DB('avtors')->hsr('uid,gender',$a);
  if ($a1->uid) {
    if ($a1->gender == 'f') $r = "Известна как ";
    elseif ($a1->gender == 'g') $r = "Известны как ";
    else $r = "Известен как ";
    $r .= ul($a1->uid)."<br>";
  }  
  else $r = '';
  setapc($r ? $r : 1);  
  return $r;
}

function libgg($b, $reset = 0) {
  if (!$b) return '';
  if ($r = getapc($b, 0)) if (!$reset) return $r == 1 ? '' : $r;
  $r = DB('genre')->get("GROUP_CONCAT(DISTINCT CAST(gid AS CHAR) ORDER BY gid)", "bid=$b");
  setapc($r ? $r : 1, 6000);
  return $r;
}  

function sl($sid) {
  if (!$sid) return '';
  if ($r = libcg("sl$sid", 100000)) return $r;
  if ($par = DB('seqs')->parent($sid)) $r = "<a href=/s/$par>".libTitle('s', $par)."</a>: ";
  $r .= "<a href=/s/$sid>".libTitle('s', $sid)."</a>";
  return libcs("sl$sid", "<h8>$r</h8>", "s$sid");
}

function gl($gid) {
  return "<a href=/g/$gid>".libgen($gid, 'gdesc'). "</a>";
}

function pl($pid) {
  return "<a href=/publisher/$pid>".DB('publishers')->title($pid)."</a>";
}
function uname($u) {
  if (!($u > 0)) return '';
  if ($name = apc_fetch("username$u")) return $name;
  $name = DB('users')->name($u);
  apc_store("username$u", $name); 
  return $name;  
}

function libTitle($type, $id) {
  if (!$id = intval($id)) return '';
  switch($type[0]) {
    case 'a': return AvtorName($id);
    case 'm': return DB('mags')->title($id);
    case 'g': return libgen($id, 'gdesc');
    case 'b': 
	  if ($a1 = DB('book')->hsr('Title,Title1', $id))
      return $a1->Title . ($a1->Title1 ? " [$a1->Title1]" : ''); 
	  return "Нет такой";
    case 's': 
	  if ($a1 = DB('seqs')->hsr('seqname,lang', $id))
        return $a1->seqname . ($a1->lang && $a1->lang != 'ru' ? " [$a1->lang]" : '');
	  break;
	case 't': return taxonomy_term_load($id)->name;  
  }  
  stop("Unsupported type $type:$id");
}

function liblink($type, $id, $edit='') {
  return "<a href=/$type/$id$edit>".libTitle($type, $id)."</a>";
  return "<a href=/".($type == 't' ? 'taxonomy/term' : $type)."/$id$edit>".libTitle($type, $id)."</a>";
}
  
function libGetnid($id, $type) {
  static $nids;	
  if (isset($nids["$type$id"])) return $nids["$type$id"];
  return $nids["$type$id"] = DB('libnode')->nid("type=? AND id=?", $type, $id) ?: 0;
}

function libListFiles($id, $type) {
  $r = [];
  if ($node = node_load(libGetnid($id, $type))) 
    if (isset($node->upload)) 
	  foreach($node->upload as $fl) 
        foreach($fl as $f) 
	      $r[] = str_replace(['public://files/', 'public://'], '/files/', $f['uri']);

  return $r;	
}


Global $langnames; 
$langnames = array('aa' => 'афар','ab' => 'абхазский','ae' => 'авестийский','af' => 'африканас','ak' => 'акан','am' => 'амхарский','an' => 'арагонский','ar' => 'арабский',
'as' => 'ассамский','av' => 'аварский','ay' => 'аймара','az' => 'азербайджанский','ba' => 'башкирский',
'be' => 'белорусский','bg' => 'болгарский','bh' => 'бихари','bi' => 'бислама','bm' => 'бамбара','bn' => 'бенгали','bo' => 'тибетский','br' => 'бретонский', 'bs' => 'боснийский',
'ca' => 'каталанский','ce' => 'чеченский','ch' => 'чаморро','co' => 'корсиканский','cr' => 'кри','cs' => 'чешский','cu' => 'церковнославянский',
'cv' => 'чувашский','cy' => 'валлийский','da' => 'датский','de' => 'немецкий','dv' => 'дивехи','dz' => 'дзонг-кэ','ee' => 'эве','el' => 'греческий',
'en' => 'английский','eo' => 'эсперанто','es' => 'испанский','et' => 'эстонский','eu' => 'баскский','fa' => 'персидский','ff' => 'фулах','fi' => 'финский',
'fj' => 'фиджи', 'fo' => 'фарерский','fr' => 'французский','fy' => 'фризский','ga' => 'ирландский','gd' => 'гэльский','gl' => 'галисийский','gn' => 'гварани',
'gu' => 'гуджарати','gv' => 'мэнский','ha' => 'хауса','he' => 'иврит','hi' => 'хинди','ho' => 'хиримоту','hr' => 'хорватский','ht' => 'гаитянский',
'hu' => 'венгерский','hy' => 'армянский','hz' => 'гереро','ia' => 'интерлингва','id' => 'индонезийский','ie' => 'интерлингве','ig' => 'игбо',
'ii' => 'сычуаньский','ik' => 'инулиак','io' => 'идо','is' => 'исландский','it' => 'итальянский','iu' => 'инуктитут','ja' => 'японский','jv' => 'яванский','ka' => 'грузинский','kg' => 'конкани','ki' => 'кикуйю','kj' => 'киньяма','kk' => 'казахский','kl' => 'эскимосский','km' => 'кхмер','kn' => 'каннада','ko' => 'корейский','kr' => 'канури','ks' => 'кашмири','ku' => 'курдский',
'kv' => 'коми','kw' => 'корнский','ky' => 'киргизский','la' => 'латынь','lb' => 'люксембургский','lg' => 'ганда','li' => 'лимбургский','ln' => 'лингала',
'lo' => 'лаосский','lt' => 'литовский','lu' => 'луба-катанга', 'lv' => 'латышский',
'mg' => 'малагасийский','mh' => 'маршальский','mi' => 'маори','mk' => 'македонский','ml' => 'малаялам','mn' => 'монгольский','mr' => 'маратхи',
'ms' => 'малайский','mt' => 'мальтийский','my' => 'бирманский','na' => 'науру','nb' => 'норвежский букмол','nd' => 'ндебеле, северный','ne' => 'непальский',
'ng' => 'ндунга','nl' => 'голландский','nn' => 'норвежский нюнорск','no' => 'норвежский','nr' => 'ндебеле, южный','nv' => 'навахо','ny' => 'ньянджа','oc' => 'окситанский','oj' => 'оджибва',
'om' => 'оромо','or' => 'ория','os' => 'осетинский','pa' => 'панджаби','pi' => 'пали','pl' => 'польский','ps' => 'пушту','pt' => 'португальский','qu' => 'кечуа',
'rn' => 'кирунди','ro' => 'румынский','ru' => 'русский','rw' => 'киньяруанда','sa' => 'санскрит','sc' => 'сардинский','sd' => 'синдхи',
'se' => 'саамский','sg' => 'санго','sk' => 'словацкий','sl' => 'словенский','sm' => 'самоанский','sn' => 'шона','so' => 'сомали',
'sq' => 'албанский','sr' => 'сербский','st' => 'сото','su' => 'сунданский','sv' => 'шведский','sw' => 'суахили','te' => 'телугу','tg' => 'таджикский','th' => 'таиландский','ti' => 'тигринья','tk' => 'туркменский','tl' => 'тагалог','to' => 'тонга','tr' => 'турецкий','ts' => 'тсонга','tt' => 'татарский',
'tw' => 'тви','ty' => 'таитянский','ug' => 'уйгурский','uk' => 'украинский','ur' => 'урду','uz' => 'узбекский','ve' => 'венда','vi' => 'вьетнамский','vo' => 'волапюк','wa' => 'валлонский','wo' => 'волоф','xh' => 'коса','yi' => 'идиш','yo' => 'йоруба','za' => 'чжуань','zh' => 'китайский','zu' => 'зулу');

// 1 - русский, 2 - русском, 3 - русского
function langname($l, $type = 2) {
  Global $langnames; 
  $l = strtolower($l);
  $nolangs = array(1 => $l, 2 => "языке $l", 3 => "языка $l");
  if (!isset($langnames[$l])) return $nolangs[$type];
  $ll = $langnames[$l];
  if ($type == 1) return $ll;
  $l2 = array('la' => 'латыни', 'he' => 'иврите', 'sa' => 'санскрите');  
  if ($type == 2) return $l2[$l] ?? str_replace('кий', 'ком', $ll);
  $l3 = array('la' => 'латыни', 'he' => 'иврита', 'sa' => 'санскрита');  
  if ($type == 3) return $l3[$l] ?? str_replace('кий', 'кого', $ll);
}

function is_lang($l) {
  if ($l == 'кг' || $l == 'ру') return 'ru';
  if ($l == 'ут') return 'en';
  if (strlen($l) != 2) return '';
  Global $langnames;
  return isset($langnames[$l]) ? $l : false;
}

function libCover($p = 0) {
  $b = rtrim($p, 't');	
  header('Content-Type: image/gif');
  if (!$b = intval($b)) {
    readfile("img/dot.gif");
    exit;
  }
  $dir = "cover/".($b % 100)."/";
  if (file_exists($dir.$p)) {
    readfile($dir.$p);
    exit;
  }
  $im = imagecreatetruecolor(99, 150);
  imagefill($im, 0, 0, 0xf0f0f0);
  $y = 0;
  foreach(explode(' ', AvtorName(libaid($b), 'lastname')) as $w) 
	imagettftext($im, 7, 0, 5, 15+($y++)*10,  0, '/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf', $w);
  foreach(explode(' ', libTitle('b', $b)) as $w) 
  	imagettftext($im, 9, 0, 5, 25+(++$y)*12, 0, '/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf', $w);
  imagegif($im);
  imagedestroy($im);
  exit;  
}

function list_subscribed() {
  $r = '';
  if ($nid = intval(arg(1))) {
    foreach(DB('{subscriptions}')->recipient_uidS("field='nid' AND value=$nid") as $u) 
      $r .= "<li>".ul($u);
  }
  return $r ? "<ol>$r</ol>" : 'Подписчиков не обнаружено';
}

function ava($a, $s1 = '', $s2 = 'а', $s3 = 'и') { 
  $aid = $a->aid ?? $a;
  if (!$sex = apc_fetch("ava $aid")) {
    $sex = ($a->gender ?? DB('avtors')->gender($aid)) ? : 1;	 
    apc_store("ava$aid", $sex);
  } 	
  if ($sex == 'f') return $s2;
  if ($sex == 'g') return $s3;
  return $s1;
}

function repeated($id) {
  static $IDS = [];	
  if (isset($IDS[$id])) return 1;	
  $IDS[$id] = 1;
  return 0;	
}

function nobooks() {
  static $host = '';
  if (user_access('download undownloadable')) return 0;
  if (!$host) $host = strpos($_SERVER['HTTP_HOST'], 'rus.ec') ? 1 : 2; 
  if ($host == 2) return 0;
  if (libProfit() > 90) return 0;
  return 1;
}

// Относительный уровень нагрузки: до 1 - сервер свободен, 1-2 - загружен, больше 2 - сильно загружен.
function LA() {
  static $LoadAvg;
  if ($LoadAvg || ($LoadAvg = apc_fetch('LoadAvg'))) return $LoadAvg;
  list($la1) = explode(' ', file_get_contents("/proc/loadavg"));
  $LoadAvg = $la1/5;
  apc_store('LoadAvg', $LoadAvg, 50);
  return $LoadAvg;
}