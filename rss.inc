<?php
/**
 * @file
 * Authors 
**/

module_load_include('inc', 'librusec');

function RSSlink($l, $tit='') {
  if ($tit) {
    drupal_add_html_head('<link rel="alternate" type="application/atom+xml;type=entry;profile=opds-catalog" title="'.$tit.'" href="http://lib.rus.ec/'.$l.'/opds" />');
    drupal_add_html_head('<link rel="alternate" type="application/rss+xml" title="'.$tit.'" href="http://lib.rus.ec/'.$l.'/rss" />');
  }
  return '<a href="'.$l.'rss"><img border=0 src=/misc/feed.png alt=RSS width=16 height=16></a>';
}

function libRSS($p) {
  if (!$uid = uid()) $uid = $p->uid;
  $gg = $uid && !$p->genre ? libListBlackGG($uid) : '';
  $name = ($p->name ?? '') ? " - $p->name" : '';
  $cid = 'RSS'.$p->uniq.get('q').$gg;
  if (!$r = libcg($cid, 3600, 1)) {
    $host = preg_replace('/^www./','',$_SERVER['HTTP_HOST']);
    $link = $p->link.$p->ltlink;
    $r = '<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0">
<channel>
<title>Либрусек'."$name</title>
<link>http://$host/$link</link>
<description>Обновления на Либрусеке$name</description>
";
    if (arg(0) == 'm') {
      $table[] = 'mag';
      $where = "mid=".floor(arg(1));
    } else {
      $table = $p->table;
      $where = $p->where;
    }  
    foreach(DB($table, 'book')->select('DISTINCT b.bid,Title,UNIX_TIMESTAMP(b.Time) AS T,FileSize', "$where$p->ltsql AND NOW()-b.Time > 10000 ORDER BY b.Time DESC LIMIT 50") as $a1) {
      $b = $a1->bid;
      $a = AvtorName(libaid($b));
      $g = libgen(DB('genre')->gid($b), 'gdesc'); 
      $extra = array(array('key' => 'pubDate', 'value' => gmdate('r', $a1->T)), array('key' => 'guid', 'value' => "http://$host/b/$b") );
      $fs = floor($a1->FileSize/1024);
      if ($fs) $fs = "($fs"."K) ";
      else $fs = '';
      $r .= format_rss_item("$a1->Title $fs- $a - $g", "http://$host/b/$a1->bid", ReadAnnotation($b), $extra);
    }
    $r .= "</channel>\n</rss>\n";      
    libcs($cid, $r);
  }
  ob_clean();
  header('Content-Type: application/rss+xml; charset=utf-8'); 
  echo $r;
  exit;
}

function libPolkaShowAllRSS($a=0) {
  Global $host;
  if ($a) $a = substr($a, 2);
  $host = preg_replace('/^www./','',$_SERVER['HTTP_HOST']);
  $cid = "p/$a.$host.rss";
  $ii = 0;
  $link = "<link>http://$host/polka/show/all</link>";
  if ($a > 0) $link = "<link>http://$host/a/$a/v</link>";
  if (!$r = libcg($cid, 3600, 1)) {
    $where = 'WHERE';
    if ($a > 0) $where = "JOIN libavtor a USING (bid) WHERE a.aid = $a AND";
    $sth = SELECT("p.bid AS bid, uid, Text, type, Flag, CONCAT(DATE_FORMAT(Time, '%a, %e '), LEFT(DATE_FORMAT(Time, '%M'),3), DATE_FORMAT(Time, ' %Y %T')) AS T, Time 
      FROM libpolka p $where Flag = '' AND Text <> '' ORDER BY pid DESC LIMIT 50");
    foreach($sth as $a1) {
      $an = libpline($a1, 1, 1);
      $book = libTitle($a1->type, $a1->bid);
      if (!$ii++) $r .= "<lastBuildDate>$a1->T GMT</lastBuildDate>\n";
      $r .= format_rss_item($book, "http://$host/$a1->type/$a1->bid", $an, 
         array('guid' => "libbook@$host/$a1->bid/$a1->uid", 'pubDate' => "$a1->T GMT"));
    }
    libcs($cid, $r);
  }
  ob_clean();
  header('Content-Type: application/rss+xml; charset=utf-8'); 
  echo '<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0">
<channel>
<title>Новые впечатления на Либрусеке</title>'." 
$link
<description>Последние 50 впечатлений о книгах на Либрусеке</description>
".$r . "</channel>\n</rss>\n";
  exit;
}
