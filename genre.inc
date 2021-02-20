<?php
module_load_include('inc', 'librusec');
module_load_include('inc', 'librusec', 'librow');
module_load_include('inc', 'librusec', 'opds');

// список жанров /g или список книг жанра /g/ЖАНР
function libGenre(string $g = '', string $act = '') : string {
  $asql = libShowEN();
  if ($g == 'opds') {
    foreach(DB('genres')->select('gid,code,gdesc', 'gid>1000') as $a1) {
	  $gids = DB('genres')->get("GROUP_CONCAT(gid)", "gidm=?", $a1->gid);
      $entrys[] = ['link' => "g/$a1->code", 'title' => $a1->gdesc, 'updated' => DB('book','genre')->Time("gid IN($gids) ORDER BY Time DESC")];
	} 
    return libOPDS('genre', 'Группы жанров', 'g', '', '', $entrys);
  }
  $r = $gidm = '';
  if (substr($g, -3) == 'all' && $act == 'opds' && ($gm = libgen($g))) { 
    foreach(DB('genres')->gidS("gidm=$gm->gid") as $gid) {
      $g = libgen($gid);
      $entrys[] = array('link'=>"g/$g->code", 'title'=>$g->gdesc, 'updated'=>DB('book','genre')->Time("gid=$g->gid ORDER BY Time DESC"));
    }
    return libOPDS('genre', $gm->gdesc, 'g', '', '', $entrys);
  }
  $u = uid();
  if (!$g || $g == 'all' || $g == 'ge') { 
    set_title("Список жанров");
    $gg = libListBlackGG();
    $cid = 'genre'.$asql.$gg.$gidm.'v'.GetUserOpt('v');
    if ($r = libcg($cid, 12*3600, 'lock')) return $r;
	if ($u) $r .= "Сортировать по ".FormSelect("useroptv", "setuseropt('v')", ['количеству книг', 'названию жанра'], GetUserOpt('v') ? [0,1] : [1,0]); 
    foreach(DB('genres')->select("gidm, SUM(1)", ($gidm?"gidm=$gidm":"gidm")." GROUP BY 1 ORDER BY 2 DESC") as $a1) {
      $meta = DB('genres')->row($a1->gidm);
	  if (!$meta) stop($a1);
      $r .= "\n<h3>".RSSlink("/g/$meta->code/")."$meta->gdesc</h3>\n<ul>\n";
      foreach(GetUserOpt('v') ?  DB('genres')->select('gid,code,gdesc', "gidm=$a1->gidm ORDER BY 3")
				  		      :  DB('genres', 'genre')->select('gid,code,gdesc,sum(1) n', "gidm=$a1->gidm GROUP BY 1,2,3 ORDER BY 4 DESC") as $g1) {
        $gc = $g1->code;
        if ($gg && DB('userblackgenre')->gid("uid=? AND gid=?", $u, $g1->gid)) {
          $r .= "\n<li><a name=$gc>$g1->gdesc ($gc) <a href=/g/$gc/show>(открыть)</a>";
        } else {
          $r .= "\n<li>".RSSlink("/g/$gc/")." <a name=$gc><a href=/g/$gc class=\"colorlnk\">$g1->gdesc</a> ($gc)";
		  if (isset($g1->n)) $r .= " - $g1->n";
          if ($u) $r .= " <a href=/g/$gc/hide>(скрыть)</a>";
        }
      }
      $r .= '</ul>';
    }  
    libcs($cid, $r, 'book');
    return $r;
  } 
  if ($g == 'null') {
    $g1 = (object)array('gid' => 0, 'code' => 'null', 'gdesc' => 'Без жанра', 'meta' => 'В работе');
  } else {
    $g1 = libgen(urldecode($g));
    if (!$g1) return check_plain("Не существует жанра $g. Извините.");
  }  
  if ($u) {
    if ($act == 'hide') {
      DB('userblackgenre')->insert(['uid' => $u, 'gid' => $g1->gid]);
      libListBlackGenre(1); //reset cache
      return "<h2>Жанр $g1->gdesc добавлен в чёрный список.</h2><a href=/g/$g1->gid/show>(удалить жанр $g1->gdesc из чёрного списка)</a> <a href=/g>(полный список жанров)</a>";
    }
	if ($act == 'show') {
	  DB('userblackgenre')->Delete('uid=? AND gid=?', $u, $g1->gid);	
      drupal_set_message("Жанр $g1->gdesc удалён из чёрного списка.");
    } elseif (DB('userblackgenre')->gid("uid=? AND gid=?", $u, $g1->gid))
      return "<a href=/g/$g1->gid/show>Удалить жанр $g1->gdesc из чёрного списка</a>";
    if ($g1->gid)  
      $r = "<a href=/g/$g1->gid/hide>(добавить жанр '<b>$g1->gdesc</b>' в чёрный список)</a> &nbsp; <a href=/g>(полный список жанров)</a><br>".$r;
  }
  $p = $g == 'null' ? libParseArgs("g/null", 'Без жанра', 'genre', "ISNULL(gid)") : 
     libParseArgs("g/$g1->code", $g1->gdesc, 'genre', ($g1->gid > 100000 ? "gid IN(".implode(',',DB('genres')->gidS("gidm=$g1->gid")).")" : "gid=$g1->gid"));
  if ($p->sorttype == 'a') $p->order = "LastName, FirstName, MiddleName, Title";
  if ($p->sorttype == '1') $p->order = "Time DESC";
  set_title($g1->gdesc.$p->lty);
  $page = $_GET['page'] ?? 0;
  if (!$page && !$p->lang) if ($ann = libReadDesc($g1->gid, 'g')) $r .= "<br>".showblocklink("blg", $g1->gid, "Описание", $_COOKIE['blg'] ?? 0 == 1 ? '' : $ann);
  $r .= downloadall('up', $p->n, $g1->code)." &nbsp; ".$p->langtype.$p->sort.AnnChkbox().$p->rdmes."<br>"; 
  if ($g1->gid)
    $ggg = DB($p->table)->paged('DISTINCT bid,Time,Year', "$p->where$p->ltsql ORDER BY $p->order");
  else   
    $ggg = DB('libbook b LEFT JOIN libgenre g USING(bid)', $p->table)->paged('bid,Time,Year', "$p->where$p->ltsql AND ISNULL(gid) ORDER BY $p->order");
  $r .= $p->tt1 ?? '';
  $oast = $oy = '';
  foreach($ggg as $a1) { 
    $hd = '';
    if ($p->sorttype == 'd' && ($tm = ptm($a1->Time))) $hd = $tm;
    if ($p->printyear && $a1->Year != $oy) {
      $oy = $a1->Year;
      $hd = "<a href=/$p->link/$oy>$oy</a>";
    }  
    if ($p->sorttype == 'a') {
      $ast = libAvtorsNames($a1->bid, 3);
      if ($oast != $ast) $hd = $oast = $ast;
    }
    if ($hd) $r .= $p->tt2 . "<h4>$hd</h4>". $p->tt1;
    $rate = $p->rd ? ' rate' : '';
    if ($p->sorttype != 'a') $rate .= ' authors ';
    $r .= DoLibRow($a1->bid, 'checkbox genre annotation sim'.$rate, 0, $p->ltlink);
  }
  return $r.$p->tt2.downloadall('dn').theme('pager');
}
