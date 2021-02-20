<?php
module_load_include('inc', 'librusec');
module_load_include('inc', 'librusec', 'librow');

// друпальный callback, обработка путей /A, /B и прочих букв и глубже
function libLetter(string $buk) : string {
  if (arg(1) == 'opds' || arg(2) == 'opds') return libOPDS('avtors', "Список авторов", $buk);
  set_title("Авторы, фамилия которых начинается на букву $buk");
  $cid = "letter$buk";
  if ($buk == 'Прочее') $W = "LastName < 'Z' "; else $W = "LastName LIKE '$buk%'";
  if (!$r = libcg($cid)) {
    foreach(DB('avtors')->aidS("$W ORDER BY books DESC LIMIT 22") as $a)
      $r .= avl($a, '&nbsp;').' &nbsp; ';
    libcs($cid, $r);  
  }
  return $r." ...<br><br>".libList('a', $buk);
}

// Страница фильтр-списка со встроенным js
// $type 'a' - список авторов, 'b' - книг
// $FirstLetter - первая буква фамили автора для списка авторов или названия книги для списка книг.
function libList(string $type = '', string $FirstLetter = '') : string {
  if ($FirstLetter && $type == 'b') {
    $FirstTitle = $FirstLetter;
    $FirstLetter = '';
  } else {
    $FirstTitle = '';
  }	
  $sln1 = $FirstLetter ? ' checked' : '';  
  $selgenre = "<center><input id=genre1 size=29> <span id=genre2></span> <span id=genre3></span><center>";
  $r = libPopup("<h1>Выбор жанра</h1>
<form action=javascript:libgenreset()>$selgenre<br><br><center><input class=button type=submit value='Выбрать'></center></form>", 'genres', 
"function libgenreset() {\$('#g').val(\$('#genre1').val());libRB();disablePopup()}");

  $r .= "<script>var page='".get('page')."'";
  $r .= '
var t1 = "<div class=fattablewrap><table><tr><td>";
var t2 = "</table></div>";
var lp = "1";
var ltm = 0;
var lp1 = "";
var type = "";
var cnt=0;
var tt;

function libRB(libnow) {
 cnt++;
 var tm = new Date; tm = tm.getTime();
 if (libnow != 1 && tm - ltm < 1555) { setTimeout(libRB, 333); return; } //были недавно, зайдите попозже.
 ltm = tm;
 var par = "";
 if (page) par = "page="+page+"&";
 var a = ["t", "g", "ln", "fn", "mn", "s1", "s2", "e", "lng", "gender", "y"];
 for (var i in a) {
   var q = jQuery("#"+a[i]).val();
   if (q) par += a[i] + "=" + encodeURI(q) + "&";
 }  
 jQuery("[name=sa],[name=rd]").find(":selected").each(function(i) {
   v = this.value
   par += v.substr(0, 2) + "=" + v.substr(3) + "&"
 });
 if (v = jQuery("[name=r]:checked").val())
   par += v + "=1&"

 console.log("librb", par)
 if (libnow != 1 && lp1 != par) {//прошлый раз просили не это. Еще не определились. Зайдите попозже.
   lp1 = par;
   setTimeout(libRB, 666);
   return;
 }
 if (lp != par) {
   jQuery("#books").html("<p>грузим...</p>");
   jQuery.get("/makebooklist?type='.$type.'&"+par, function(r){ jQuery("#books").html(t1+r+t2); } ) 
   lp = par;
 }  
}
function pg(p) {page=p; libRB(1);}
</script> 
<style>th {font-size: 70%;padding:0px;spacing:0px}</style>'."<a href=javascript:loadPopup('genres')>Жанры</a>, ";
  $r .= showinfolink(1, 8, 'языки', 'div') . ' и ' . showinfolink(1, 9, 'форматы', 'div') . " можно указывать через запятую.";
  $r .= "
<style>th,td{text-align:center;white-space: no-wrap;}</style>
<div class=fattablewrap>
<table id=tt><tr>";
  if ($type != 'a') $r .= "<th>Название";
  $r .= "
<th>Фамилия<th>Имя<th>Отчество<th>Жанр<th>Год<th>Размер<th>Формат<th>Язык<th>Дата<th>Пол
<tr>";
  $issmtschecked = 0;
  foreach(array('st', 'sln', 'sfn', 'smn', 'sg', 'ss', 'se', 'sd', 'sy') as $ss) {
    ${$ss.'1'} = get($ss.'1') ? "CHECKED" : ''; 
    ${$ss.'2'} = get($ss.'2') ? "CHECKED" : ''; 
	$issmtschecked++;
  }  
  if (!$issmtschecked) if ($type == 'a') $sln1 = "CHECKED"; else $st1 = "CHECKED"; 
  if ($type != 'a') 
    $r .= "<td nowrap>
<input type=radio name=r value=st1 $st1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=st2 $st2 onchange=libRB()><img src=/img/s2.gif>";
$r .= "
<td nowrap>
<input type=radio name=r value=sln1 $sln1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=sln2 $sln2 onchange=libRB()><img src=/img/s2.gif>  
<td nowrap>
<input type=radio name=r value=sfn1 $sfn1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=sfn2 $sfn2 onchange=libRB()><img src=/img/s2.gif>  
<td nowrap>
<input type=radio name=r value=smn1 $smn1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=smn2 $smn2 onchange=libRB()><img src=/img/s2.gif>  
<td nowrap>
<input type=radio name=r value=sg1 $sg1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=sg2 $sg2 onchange=libRB()><img src=/img/s2.gif>  
<td nowrap>
<input type=radio name=r value=sy1 $sy1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=sy2 $sy2 onchange=libRB()><img src=/img/s2.gif>
<td nowrap>
<input type=radio name=r value=ss1 $ss1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=ss2 $ss2 onchange=libRB()><img src=/img/s2.gif>
<td nowrap>
<td nowrap>
<td nowrap>
<input type=radio name=r value=sd1 $sd1 onchange=libRB()><img src=/img/s1.gif>
<input type=radio name=r value=sd2 $sd2 onchange=libRB()><img src=/img/s2.gif>
<td nowrap>
<tr>";

  foreach (array('tm', 't', 'ln', 'fn', 'mn', 'g', 'e', 's1', 's2', 'lng', 'gender', 'y') as $ss) $$ss = check_plain(get($ss));

  if (!$FirstTitle) $FirstTitle = $t;
  if (!$FirstLetter) $FirstLetter = $ln;
  if ($type != 'a') $r .= "
<td><input id=t size=8 onKeyUp=libRB() value='$FirstTitle'>";
  $r .= "
<td><input id=ln size=7 onKeyUp=libRB() value='$FirstLetter'>
<td><input id=fn size=6 onKeyUp=libRB() value='$fn'> 
<td><input id=mn size=5 onKeyUp=libRB() value='$mn'>
<td><input id=g size=5 onKeyUp=libRB() value='$g'>
<td><input id=y size=5 onKeyUp=libRB() value='$y'>
<td nowrap><input id=s1 size=1 onKeyUp=libRB() value='$s1'>K&mdash;<input id=s2 size=1 onKeyUp=libRB() value='$s2'>K
<td><input id=e size=4 onKeyUp=libRB() value='$e'>
<td><input id=lng size=2 onKeyUp=libRB() value='$lng'>
<td>
<td><select id=gender onchange=libRB()><option value=''>Все
  <option value=f".($gender == 'f' ? ' selected' : '').">Женщина
  <option value=m".($gender == 'm' ? ' selected' : '').">Мужчина
  <option value=g".($gender == 'g' ? ' selected' : '').">Группа
  <option value=u".($gender == 'u' ? ' selected' : '').">неизвестен
".tt2;
  $r .= "</form>\n<div id=books><br></div>";
  $r .= "<script>jQuery(document).ready(libRB)</script>"; 
  return $r;
}

// AJAX-callback, формирует список книг согласно параметрам из URL. Вызывается при любых изменениях опций фильтр-списка
function libAJAXlist() : string {
  $p = libParseArgs();
  $cid = 'ajaxlist-'.serialize($_GET).$p->uniq;
  if ($r = libcg($cid, 55555)) pe($r);
  $tables = ['avtor'];
  $W = $p->where;
  $pure_a = 1;
  if ($t = p('t')) $W .= " AND Title LIKE '".str_replace('*','%',$t)."%'";
  if ($t = p('ln')) $W .= " AND LastName ".($t == 'Прочее' ?  "< 'Z' " : "LIKE '$t%'");
  elseif (get('sln1') || get('sln2')) $W .= " AND LastName"; 
  if ($t = p('fn')) $W .= " AND FirstName LIKE '$t%'";
  elseif (get('sfn1') || get('sfn2')) $W .= " AND FirstName"; 
  if ($t = p('mn')) $W .= " AND MiddleName LIKE '$t%'";
  elseif (get('smn1') || get('smn2')) $W .= " AND MiddleName"; 
  if ($t = p('g')) {
    $wst = '';
    foreach (explode (',', $t) as $l) {
      if ($l = addslashes(trim($l))) {
        if ($wst) $wst .= " OR";
        $wst .= " gdesc LIKE '%$l%' OR code LIKE '$l%'";
      }  
    }
    $W .= " AND ($wst)";
	$pure_a = 0;
  }  
  if ($t = p('e')) {
    $wst = '';
    foreach (explode (',', $t) as $l) {
      if ($l = trim($l)) {
        if ($wst) $wst .= " OR";
        $wst .= " FileType = '$l'";
      }  
    }
    $W .= " AND ($wst)";
	$pure_a = 0;
  } 
  if ($t = p('s1')) $W .= " AND FileSize >= ".($t*1024);
  if ($t = p('s2')) $W .= " AND FileSize <= ".($t*1024);
  if ($t = p('lng')) {
    $wst = '';
    foreach (explode (',', $t) as $l) {
      if ($l = trim($l)) {
        if ($wst) $wst .= " OR";
        if ($l == 'NULL') $wst .= " b.Lang = ''";
        else $wst .= " b.Lang = '$l'";
      }  
    }
    $W .= " AND ($wst)";
  	$pure_a = 0;
  }
  if ($t = p('y')) {
    $wst = '';
    foreach (explode (',', $t) as $l) {
      if ($l = trim($l)) {
        if ($wst) $wst .= " OR";
		$yy = explode('-', $l);
        if (isset($yy[1])) $wst .= " Year >= ".intval($yy[0])." AND Year <=".intval($yy[1]);
        else $wst .= " Year = ".intval($yy[0]);
      }  
    }
    $W .= " AND ($wst)";
	$pure_a = 0;
  }
  if ($t = p('gender')) {
	$W .= " AND gender = '".($t == 'u' ? '' : $t)."'";
	$pure_a = 0;
  }
  if (get('st1')) $O = "Title"; 
  elseif (get('st2')) $O = "Title DESC"; 
  elseif (get('sln1')) $O = "LastName, FirstName, MiddleName"; 
  elseif (get('sln2')) $O = "LastName DESC, FirstName, MiddleName"; 
  elseif (get('sfn1')) $O = "FirstName, LastName, MiddleName"; 
  elseif (get('sfn2')) $O = "FirstName DESC, LastName, MiddleName"; 
  elseif (get('smn1')) $O = "MiddleName, LastName, Firstname"; 
  elseif (get('smn2')) $O = "MiddleName DESC, LastName, Firstname"; 
  elseif (get('sg1')) $O = "gdesc"; 
  elseif (get('sg2')) $O = "gdesc DESC"; 
  elseif (get('ss1')) $O = "FileSize"; 
  elseif (get('ss2')) $O = "FileSize DESC"; 
  elseif (get('se1')) $O = "FileType"; 
  elseif (get('se2')) $O = "FileType DESC"; 
  elseif (get('sd1')) $O = "bid"; 
  elseif (get('sd2')) $O = "bid DESC"; 
  elseif (get('sy1')) $O = "Year, Title"; 
  elseif (get('sy2')) $O = "Year DESC, Title DESC"; 
  else $O = 'bid DESC';

  foreach(['st', 'ss', 'se', 'sd', 'sy', 's', 't'] as $qq) 
    if (get($qq) || get('s'.$qq.'1') || get('s'.$qq.'2')) 
	  $pure_a = 0;	

  foreach (array('ln', 'fn', 'mn', 'gender') as $qq)  
    if (get($qq) || get('s'.$qq.'1') || get('s'.$qq.'2')) 
      $tables[1] = 'avtors';
  if (get('g') || get('sg1') || get('sg2')) { 
    $tables[2] = 'genre';
    $tables[3] = 'genres';
  }	  
  $r = "<div align=left>";
  $ll = '';  
  foreach($_GET as $pp => $vv) if ($pp != 'q' && $pp != 'type') $ll .= "$pp=$vv&";
  $ll = rtrim($ll, '&');
  if (get('type')=='a' && $pure_a) {
	$W .= " AND NOT main";
	if ($O === 'bid DESC') $O = 'books DESC';
	$books = DB('avtors', $tables)->paged('aid, books', "$W GROUP BY 1,2 ORDER BY $O ");
    foreach($books as $a1) {
	  $r .= avl($a1->aid, ' ', 1);
	  if ($a1->books) $r .= " ($a1->books)";
	  $r .= "<br>\n";
	}  
  } else { 
    $W .= ' AND NOT Deleted AND NOT ReplacedBy';
    if (get('type')=='a') {
      $r .= " Ссылка: <a href=\"/a/?$ll\">http://lib.rus.ec/a/?$ll</a><br>";
      $tables[0] = 'avtor';
	  $books = DB('book', $tables)->paged("a.aid, COUNT(b.bid) as N", "$W GROUP BY 1 ORDER BY $O");
      foreach($books as $a1) $r .= avl($a1->aid, ' ', 1)." ($a1->N)<br>\n";
    } else { 
      if (!isset($tables[1])) unset($tables[0]);
	  if (isset($tables[2])) $W .= " GROUP BY 1";
      $books = DB('book', $tables)->paged("b.bid", "$W ORDER BY $O");
  	  $r .= downloadall('up') . ' ' . AnnChkbox('', 'libRB()');
      $r .= " Ссылка: <a href=\"/b/?$ll\">http://lib.rus.ec/b/?$ll</a><br>";
      foreach($books as $bid) $r .= DoLibRow($bid, 'authors checkbox genre annotation list '); 
	  $r .= downloadall('down')."</form>";  
    }
  }	
  $r .= "</div><br>";
  if ($books) {
    $pager = theme('pager');
    $pager = preg_replace('|/makebooklist.page=(\d+)"|','javascript:pg($1)"',$pager);
    $pager = preg_replace('|/makebooklist.page=(\d+)[^"]+"|','javascript:pg($1)"',$pager);
    $pager = preg_replace('|/makebooklist[^"]*|','javascript:pg(0)',$pager);
    $r .= $pager;
  } else {  
    $r = "<h2>Не нашлось ни единой книжки, удовлетворяющей вашим требованиям. Упростите.</h2>";
  }  
  pe(libcs($cid, $r, 'b'));
}

