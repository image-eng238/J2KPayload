# 進捗

## 2026/04/07

パケットの受信処理を行うスレッドを**受信スレッド**， パケットの解析処理を行うスレッドを**解析スレッド**と呼ぶことにする．
学士の卒業研究で作成した HTJ2K パケット用のパケット解析を行うプログラムを単に**パケット解析器**と呼ぶ．

### プログラム 
HTJ2K パケットを解析するプログラムを作成．  
RTP パケットを解析するプログラムを作成．  
上記で作成した関数を使用して， HTJ2K パケットと RTP パケットの両方を解析するプログラムを作成．
.rtp ファイルを指定のアドレスとポートに送る， kakadu の kdu_stream_send に似たツールを実験用に作成．  

### テスト

動作確認のために RTP パケットの間隔を十分に空けて，HTJ2K パケットの解析結果のログを取った．  
ログを動作確認済みの HTJ2K パケット解析器のログと比較．  
結果，解析処理は正しく動作した．  

性能評価として kakadu の kdu_stream_send と自作したツールの両方で 60fps 相当でパケットを送信したところ，
RTP パケットのシーケンス番号の欠落が起こった．  
したがって， 60fps の映像には解析速度が不十分であることがわかった．

### 考察

解析速度のボトルネックをいくつか考える．
- パケットを跨いで符号化データが送信されるが，
後続のハードウェアデコーダでの処理を見越して別のバッファにデータをコピーしている．
- HTJ2K パケットヘッダの解析用にバッファから n byte または n bit ごとに取り出す関数を使用しているが，パケットヘッダについても RTP パケットと跨ぐため，データのアクセス前にチェック処理が入る．
- 上記の関数では，範囲外にアクセスしようとしたときに， socket2.h, recv() 関数を実行した後で，参照するバッファを変更するため直前の処理時間の影響を受けやすい．
RTP は UDP 上でのプロトコルのため，直前の処理に時間がかかっているとパケットが欠落し，結果として RTP のシーケンス番号のズレにつながる．
- perf の結果からはメモリのコピー， get_bit() 関数， read_packet_header() 関数に時間がかかってるらしい．ただ perf の結果では一番パーセンテージが大きい関数でも 5% 前後．

改善点として， RTP パケットの受信処理は非同期処理にする．メモリのコピーも場合によってはスレッドを分ける．

## 2026/04/08

### テスト

上記でのメモリコピーの処理をコメントアウトし，同様の実験を行ったところ，エラーが出たパケットの位置に変化はなかった．
したがって，パケットの受信タイミングに原因があると考えられる．

### 考察

現状の RTP パケット受信のタイミングは HTJ2K パケットの解析処理に埋め込まれているため，受信のタイミング次第でパケットが欠落していると考える．  
RTP パケットの受信と HTJ2K パケットの解析を非同期を非同期処理に分離することで，この問題に対処する．

## 2026/04/13

### プログラム

他のディレクトリで非同期のバッファ処理のたたき台を作り， branch fuature-thread に持ってきた．
この処理は **leaky bucket model** と呼ばれる方式である．
バッファの書き込み待ちの処理に 
```C++
while (next_pop->empty()); // データの排出が入力より速いため，データが入力されるまで待機
```
と書いたため最適化で待機処理が消された．copilotくんいわく，上記の処理は最適化の際に消されるらしい(コンパイラは別スレッドのことを知らないため，値が変わらない or 無限ロープと見なす)．  
一部の変数を atomic 操作に変更したところ，ログ出力なし -O3でたまに動くようになった．

### 考察

[条件変数の利用方法](https://cpprefjp.github.io/article/lib/how_to_use_cv.html) によると，
上記のような実装は**ポーリング方式**と呼ばれ，参照側スレッド(今回ではパケット解析器本体側)が常に動作しリソースを消費するため，
実行効率が著しく下がるらしい．  
実際に perf を使ったところ，上記の while 文にかなりの cpu を使っている可能性があった．  
解決策として，mutex と condition_variable を用いた std::condition_variable::wait での実装に切り替える．

## 2026/04/14

### プログラム

他のディレクトリでステート方式のバッファを作り， branch fuature-thread に持ってきた．
[cpprefjp の説明](https://cpprefjp.github.io/article/lib/how_to_use_cv.html)にあやかって**ステート方式**と呼ぶことにする．
バッファ自体のテストは正しく動いている．
しかし，バッファの中身のデータ(受信したパケットのデータ)は同期処理を行っていない．
そのためバッファのデータの処理中にデータを他スレッドに書き換えらているような挙動を取った．
その根拠として，empty packet でプログラムが停止した時のデバッグ用変数 call_count が毎回異なる値を取っており，
なおかつ，RTP の extended_sequence_number を用いたアサーションではプログラムが停止しないためである．
また，current_num_data < NUM_BUFFER のアサーションに失敗している．

***追記1*** 追加の検証として，バッファからの排出処理の同期にタイムアウト(1ms)を設定したところ，
明らかに解析スレッド側が過剰にブロッキングされてことが確認できた．

***追記2*** さらに解析スレッドの処理をデータの取りだしのみに変更し， RTP の extended_sequence_number の値が飛んだときの値を観察した．
受信スレッドの while ループを短く(10us)スリープさせたところ， extended_sequence_number は 200~600 前後の範囲で飛んだ．
受信スレッドの while ループをスリープさせずに，同様の実験をしたところ， 10~90 前後の範囲で値が飛んだが，一度 2001 の値で飛んだ．


### 考察

今回の問題は，バッファのデータの寿命(ライフタイム)がパケット処理のスレッドの動作よりも短いことが原因だと考えられる．  
考えられる解決策として，

- ~~受信用のバッファを更に大きくすることで，データの寿命を延ばす．~~
- 一時的にスタックにデータを移すことで，寿命を延ばす．
- 根本的な解決策として，パケット処理側をさらに高速化させる．
- ***追記1*** 適切に同期処理を行い過剰なブロッキングが起こらないようにする．

などが考えられる．

１つ目の解決策では，解析スレッドの処理速度が受信スレッドよりも遅い場合，
データの排出が間に合わず，バッファの不足を遅らせるだけで，解決にはならない．

***追記1*** 原因として考えられることをいくつか上げると，

- 受信スレッドは最小限の while ループと受信用関数を用いており，
mutex::unlock の呼び出し後に再度 mutex::lock を呼び出すまでの間隔が短すぎる．
- ステート方式のバッファのテストが不完全・不十分．
- mutex の所有権を持った状態で socket2.h/recv 関数を呼び出している．

まず考えられる解決策として，受信スレッドの while ループに sleep 関数を組み込んで解析スレッド側のブロッキングの解除を行えるようにする．
ただし，UDP を受信する都合上，次の UDP パケットの到着までに再度 socket2.h/recv 関数で待機する必要があるため慎重に実装する必要がある．

また mutex を各要素ごとに管理する方もあるが， std::mutex のサイズを加味するとメモリ使用量が気になってくる．

mutex の所有権を持った状態で socket2.h/recv 関数を呼び出すということは，パケットを受信するまで，
mutex の所有権を手放さないということになる．
つまり，解析スレッド側で mutex::lock を呼び出している場合，パケットを受信するまで解析スレッドがブロックされてしまう．

***追記2*** このときの，受信用のバッファの最大数は2001で設定していた．
つまり， extended_sequence_number の値がバッファ一周分飛んだことになる．  
したがって，完全に解析スレッド側が過剰にブロッキングされている．
その原因は上記でも考察したように，受信スレッド側の mutex の所有時間が長過ぎることだと考えられる．

## 2026/04/15

### プログラム

2026/04/14 の考察で挙げた mutex のロック時間の問題を改善した．
今までは， mutex のロックを行った後に，解析スレッドと共有するバッファを用いて socket2.h/recv 関数を呼び出していた．
改善後は， socket2.h/recv 関数の呼び出しにテンポラリ用のバッファを用いて，
mutex のロック後に解析スレッドと共有するバッファに**コピー(std::memcpy)** を行うように変更した．  
この変更によって，**マシンパワーがある場合に限っては** 60fps の解析が可能となった．
しかし，1つの RTP パケット毎に最大 1384 byte のコピーが発生するのは無視できないボトルネックになる可能性がある．

perf を用いた計測(表1)では J2kBuf::get_bit(const uint8_t&) 関数に約 15% ， J2kBuf::get_bit() 関数に約 12% ほどの オーバヘッドが発生していることもわかった．
また カーネル処理の _copy_to_iter 関数が約 5% ほどのオーバーヘッドを示した．

#### 表1 perf report から一部抜粋 
|Overhead|Command|Shared Object|Symbol|
|---|---|---|---|
|14.95%|j2kpay|j2kpay[.]|J2kBuf::get_bit(unsigned char const&)|
|11.75%|j2kpay|j2kpay[.]|J2kBuf::get_bit()|
|5.48%|j2kpay|[kernel.kallsyms][k]| _copy_to_iter|
|5.42%|j2kpay|j2kpay[.]|PrecinctSubband::read_packet_header(J2kBuf*, unsigned char)|
|3.70%|j2kpay|j2kpay[.]|leaky_bucket_buf::pop(unsigned char*&)|

### 考察

perf report の結果から，まずは J2kBuf::get_bit 関数の処理を改善するはスループットの向上に必須であることがわかる．
解析スレッドの処理は基本的には，パケット解析器のソースコードを用いている．
しかし，J2kBuf::get_bit 関数(J2kBuf::get_byte関数も同様)では，
RTP パケットに分割された HTJ2K パケットを解析処理するために，終端時にバッファを更新する機能やそれに伴う終端チェックなどで処理時間が増加している可能性がある．
(バッファ更新が実際に J2kBuf::get_bit() 関数に含まれ計測されているかは，未検証)

#### 表2 perf report から j2kpay の抜粋

|Overhead|Comman|Symbol|
|---|---|---|
|14.95%|j2kpay[.]|J2kBuf::get_bit(unsignedcharconst&)
|11.75%|j2kpay[.]|J2kBuf::get_bit()
|5.42%|j2kpay[.]|PrecinctSubband::read_packet_header(J2kBuf*,unsignedchar)
|3.70%|j2kpay[.]|leaky_bucket_buf::pop(unsignedchar*&)
|1.87%|j2kpay[.]|Tile::read_packet(Precinctconst*,J2kBuf&)
|1.28%|j2kpay[.]|J2kBuf::make_packet_data(unsignedlongconst&,unsignedchar*)
|1.04%|j2kpay[.]|leaky_bucket_buf::receive()
|1.03%|j2kpay[.]|PrecinctSubband::init(Postion2D<unsignedint>const&,Postion2D<unsignedint>const&,Postion2D<unsignedint>const&,unsignedchar,unsignedchar,MultiMem::static_memory<2000000ul>*)
|0.89%|j2kpay[.]|Resolution::set_precinct(Postion2D<unsignedint>const&,Postion2D<unsignedint>const&,unsignedchar,MultiMem::static_memory<2000000ul>*)
|0.79%|j2kpay[.]|Tile::read(MainHeaderconst&,std::array<Precinct*,5670ul>&)
|0.76%|j2kpay[.]|CodeBlock::set_data(J2kBuf*)
|0.75%|j2kpay[.]|Resolution::empty()const
|0.70%|j2kpay[.]|Component::ceil_N_L(unsignedchar&,unsignedchar&)const[clone.part.0]
|0.60%|j2kpay[.]|Precinct::init(Subbandconst*,unsignedchar,Postion2D<unsignedint>const&,Postion2D<unsignedint>const&,Postion2D<unsignedint>const&,unsignedint,unsignedchar,unsignedchar,MultiMem::static_memory<2000000ul>*)
|0.55%|j2kpay[.]|Precinct::get_psubband_ptr(unsignedchar)const
|0.25%|j2kpay[.]|main::{lambda()#1}::operator()()const

# メモ

プロデューサーなんとか  
leaky bucket model  
RFC 9298  
precinct の数え方，再同期ポイント，ハードウェアデコーダに流せる最小単位が RTP パケットからわかるらしい 
-> そこを目安に処理  
受信部分のマルチスレッド化は必須
フレームポインタ
