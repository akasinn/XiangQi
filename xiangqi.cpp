#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip> // setprecision ?

using namespace std;


// 1文字の char32_t を UTF-8の std::string に変換する便利関数、Geminiに書いてもらった
std::string char32_to_utf8(char32_t c) {
    std::string u8str;
    if (c <= 0x7F) {
        // 1バイト文字 (半角英数字など)
        u8str += static_cast<char>(c);
    } else if (c <= 0x7FF) {
        // 2バイト文字
        u8str += static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
        u8str += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c <= 0xFFFF) {
        // 3バイト文字 (一般的な日本語など)
        u8str += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
        u8str += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        u8str += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c <= 0x10FFFF) {
        // 4バイト文字 (一部の漢字や絵文字など)
        u8str += static_cast<char>(0xF0 | ((c >> 18) & 0x07));
        u8str += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        u8str += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        u8str += static_cast<char>(0x80 | (c & 0x3F));
    }
    return u8str;
}

string u32string_to_string(u32string str){
    string ans="";
    for(char32_t c:str){
        ans+=char32_to_utf8(c);
    }
    return ans;
}


constexpr int H=12;
constexpr int W=11;
vector<int> PieceValue(200,0);//駒の価値、赤駒はマイナスの価値を持つ

struct move_t{
    int now;//移動前
    int next;//移動先
    int ds;//移動による評価値の変化

    bool equal(const move_t &o){
        return (now==o.now) && (next==o.next);
    }

    //コンストラクタでdsを計算してもいいかも、合法手か判定するのは厳しそう
    //コンストラクタではエラーになったので、メンバ関数でdsを計算
    void calc_ds(const string &board){
        const char target=board[next];
        ds=-PieceValue[target];
        if(target=='s' && next>6*W)ds-=2;
        else if(target=='S' && next<6*W)ds+=2;//敵陣の兵卒の価値は二倍
        if(board[now]=='s' && now/W==5)ds+=2;
        else if(board[now]=='S' && now/W==6)ds-=2;//川を渡ると価値が倍になる
        return;
    }
};


namespace library{

/*
=== C++ シャンチー ===

　　　１　２　３　４　５　６　７　８　９ (Ｘ座標)

　０　＋ー＋ー＋ー士ー将ー士ー象ー馬ー＋
　　　｜　｜　｜　｜＼｜／｜　｜　｜　｜
　１　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋
　　　｜　｜　｜　｜／｜＼｜　｜　｜　｜
　２　＋ー砲ー＋ー＋ー＋ー＋ー＋ー砲ー＋
　　　｜　｜　｜　｜　｜　｜　｜　｜　｜
　３　卒ー＋ー卒ー＋ー卒ー＋ー卒ー＋ー卒
　　　｜　｜　｜　｜　｜　｜　｜　｜　｜
　４　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋
　　　｜　　　　楚河　　　漢界　　　　｜
　５　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋
　　　｜　｜　｜　｜　｜　｜　｜　｜　｜
　６　兵ー＋ー兵ー＋ー兵ー＋ー兵ー＋ー兵
　　　｜　｜　｜　｜　｜　｜　｜　｜　｜
　７　＋ー炮ー＋ー＋ー＋ー＋ー＋ー＋ー＋
︵　　｜　｜　｜　｜＼｜／｜　｜　｜　｜
Ｙ８　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋
座　　｜　｜　｜　｜／｜＼｜　｜　｜　｜
標９　車ー馬ー相ー仕ー帥ー仕ー相ー馬ー車
︶　
*/

    string YOU_WIN="\n\x1b[33m\x1b[1m  ==  YOU WIN  ==  \x1b[39m\x1b[0m";
    string YOU_LOSE="\n\x1b[34m\x1b[1m  ==  YOU LOSE  ==  \x1b[39m\x1b[0m";

const u32string output_board=U"\033[90m"s+
U"　　　１　２　３　４　５　６　７　８　９ (Ｘ座標)\n"s+
U"\n"s+
U"　０　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜＼｜／｜　｜　｜　｜\n"s+
U"　１　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜／｜＼｜　｜　｜　｜\n"s+
U"　２　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜　｜　｜　｜　｜　｜\n"s+
U"　３　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜　｜　｜　｜　｜　｜\n"s+
U"　４　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　　　　楚河　　　漢界　　　　｜\n"s+
U"　５　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜　｜　｜　｜　｜　｜\n"s+
U"　６　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"　　　｜　｜　｜　｜　｜　｜　｜　｜　｜\n"s+
U"　７　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"︵　　｜　｜　｜　｜＼｜／｜　｜　｜　｜\n"s+
U"Ｙ８　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー次\n"s+
U"座　　｜　｜　｜　｜／｜＼｜　｜　｜　｜\n"s+
U"標９　＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋ー＋\n"s+
U"︶　\033[0m\n"s;


const string Initial_board=
    "WXXXXXXXXX"s + "X" +
    "XrheagaehrX"+ // 0行目: 黒陣
    "X.........X"+ // 1行目
    "X.c.....c.X"+ // 2行目
    "Xs.s.s.s.sX"+ // 3行目
    "X.........X"+ // 4行目: 楚河
    "X.........X"+ // 5行目: 漢界
    "XS.S.S.S.SX"+ // 6行目
    "X.C.....C.X"+ // 7行目
    "X.........X"+ // 8行目
    "XRHEAGAEHRX"+ // 9行目: 紅陣
    "XXXXXXXXXXX";
/*盤の周囲は壁で囲い、Xで表す
  左上の文字は手番を表し、'W'なら紅(先手、大文字)を示す、'b'なら黒(後手、小文字)を示す
  //右上の文字は手数を表す。
  */

  //経由地点と目標地点のペア
    vector<pair<int,int>> Xiang_move={{1+W,2+2*W},{1-W,2-2*W},{-1+W,-2+2*W},{-1-W,-2-2*W}};
    vector<pair<int,int>> Ma_move={{1,2+W},{1,2-W},{-1,-2+W},{-1,-2-W},{-W,-2*W+1},{-W,-2*W-1},{W,2*W+1},{W,2*W-1}};
    vector<int> Upper_Shi_move={W+4,W+6,3*W+4,3*W+6};
    vector<int> Lower_Shi_move={8*W+4,8*W+6,10*W+4,10*W+6};
    vector<int> King_move={1,-1,W,-W};

    bool is_in_casle(int a){
        return (a<4*W || 8*W<a) &&  a%W<<7 && 3<a%W;
    }

    
}//library end

//駒の色を判定する
bool is_red(const char c){
    return static_cast<unsigned char>(c - 'A') <26;
}

//大文字小文字が異なるならtrue
bool are_enemy(char c1,char c2){
    return ((c1 ^ c2) & 0x20);
}

// アルファベットを「漢字」と「色」に変換して返す関数
u32string getPieceString(char c) {
    u32string grayen = U"\033[90m";
    u32string reden = U"\033[31m";
    u32string color_reset = U"\033[0m";
    switch(c) {
        // 黒(コンピュータ)
        case 'g': return color_reset+U"将"+ grayen;
        case 'a': return color_reset+U"士"+ grayen;
        case 'e': return color_reset+U"象"+ grayen;
        case 'h': return color_reset+U"馬"+ grayen;
        case 'r': return color_reset+U"車"+ grayen;
        case 'c': return color_reset+U"砲"+ grayen;
        case 's': return color_reset+U"卒"+ grayen;
        
        // 紅(プレイヤー)
        case 'G': return reden + U"帥" + grayen;
        case 'A': return reden + U"仕" + grayen;
        case 'E': return reden + U"相" + grayen;
        case 'H': return reden + U"馬" + grayen;
        case 'R': return reden + U"車" + grayen;
        case 'C': return reden + U"炮" + grayen;
        case 'S': return reden + U"兵" + grayen;
        
        //駒が置かれていない
        case '.': return U"＋";

        default:  return U"壁";
    }
}

// 盤面を描画する関数
void printBoard(const string &board) {
    int i=W+1;//よくわからない
    for(char32_t c:library::output_board){
        if(c==U'＋'){
            cout<<u32string_to_string(getPieceString(board[i]));
            i++;
        }else if(c==U'次'){
            cout<<u32string_to_string(getPieceString(board[i]));
            i+=3;
        }else{
            cout<<char32_to_utf8(c);
        }
    }
    return; 
}

//駒を動かす、盤面遷移はO(1)でやりたいが、これはO(HW);
string move_a_piece(const move_t move,const string &board){
    string ans=board;
    ans[move.next]=board[move.now];
    ans[move.now]='.';
    ans[0]='W'+'b'-board[0];
    return ans;
}

bool king_collision(const string &Board,const move_t move){
    int x=move.now%W, x_=move.next%W;
    if(x==x_)return false;
    if((x<4 || x>6) && (x_<4 || x_>6))return false;
    const string board=move_a_piece(move,Board);
    for(int j=4;j<7;j++){
        bool b=false;
        for(int i=1;i<H-1;i++){
            char piece=board[i*W+j];
            if(b){
                if(piece!='.'){
                    if(piece=='G' || piece=='g')return true;
                    break;
                }
            }else{
                if(piece=='G' || piece=='g')b=true;
            }
        }
    }
    return false;//王は向かい合っていない
}

vector<move_t> f_all_options(const string &board){
    vector<move_t>ans(0);
    const bool teban=is_red(board[0]);
    for(int i=1;i<H-1;i++){
        for(int j=1;j<W-1;j++){
            vector<int>Targets(0);//目標の場所をどんどん入れていく
            const int now=i*W+j;//i,jを使わずにnowでforを回してもいいかも
            const char piece=board[now];
            if(piece=='.' || is_red(piece)!=teban)continue;//手番のプレイヤーの駒ではない
            if(piece=='S'){//兵
                Targets.push_back(now-W);
                if(i<6){
                    Targets.push_back(now+1);
                    Targets.push_back(now-1);
                }
            }else if(piece=='s'){//卒
                Targets.push_back(now+W);
                if(i>5){
                    Targets.push_back(now+1);
                    Targets.push_back(now-1);
                }
            }else if(piece=='R' || piece=='r'){//車
                for(int next=now-W;next>W;next-=W){
                    char target=board[next];
                    if(target=='.'){
                        Targets.push_back(next);
                    }else if(are_enemy(piece,target)){
                        Targets.push_back(next);
                        break;
                    }else break;
                }
                for(int next=now+W;next<W*(H-1);next+=W){
                    char target=board[next];
                    if(target=='.'){
                        Targets.push_back(next);
                    }else if(are_enemy(piece,target)){
                        Targets.push_back(next);
                        break;
                    }else break;
                }
                for(int next=now+1;next%W<W-1;next++){
                    char target=board[next];
                    if(target=='.'){
                        Targets.push_back(next);
                    }else if(are_enemy(piece,target)){
                        Targets.push_back(next);
                        break;
                    }else break;
                }
                for(int next=now-1;next%W>0;next--){
                    char target=board[next];
                    if(target=='.'){
                        Targets.push_back(next);
                    }else if(are_enemy(piece,target)){
                        Targets.push_back(next);
                        break;
                    }else break;
                }

            }else if(piece=='H' || piece=='h'){//馬
                for(const pair<int,int>p:library::Ma_move){
                    if(board[now+p.first]=='.')//経由地点は空白でなければならない
                        Targets.push_back(now+p.second);
                }
            }else if(piece=='E' || piece=='e'){//象/相
                for(const pair<int,int>p:library::Xiang_move){
                    //経由地点は空白でなければならない
                    //川を渡っては行けない
                    if(board[now+p.first]=='.'  &&  (now<6*W)==(now+p.second<6*W))
                        Targets.push_back(now+p.second);
                }
            }else if(piece=='A' || piece=='a'){//士
                if(now<6*W){
                    if(now==2*W+5){//お城の中央にいる
                        for(const int next:library::Upper_Shi_move)Targets.push_back(next);
                    }else{//お城の端っこにいる
                        Targets.push_back(2*W+5);
                    }
                }else {
                    if(now==9*W+5){//お城の中央にいる
                        for(const int next:library::Lower_Shi_move)Targets.push_back(next);
                    }else{//お城の端っこにいる
                        Targets.push_back(9*W+5);
                    }
                }
            }else if(piece=='G' || piece=='g'){//将
                int next=now+1;
                if(now%W<6)Targets.push_back(now+1);
                if(4<now%W)Targets.push_back(now-1);
                if(now<3*W)Targets.push_back(now+W);
                if(8*W<now)Targets.push_back(now-W);
            }else if(piece=='C' || piece=='c'){//砲
                bool b=true;//まだ駒を飛び越えていない
                for(int next=now+1;next%W<W-1;next++){
                    char target=board[next];
                    if(b){
                        if(target=='.')Targets.push_back(next);
                        else b=false;
                    }else{
                        if(target!='.'){
                            if(are_enemy(piece,target))Targets.push_back(next);
                            break;
                        }
                    }
                }
                b=true;//まだ駒を飛び越えていない
                for(int next=now-1;next%W>0;next--){
                    char target=board[next];
                    if(b){
                        if(target=='.')Targets.push_back(next);
                        else b=false;
                    }else{
                        if(target!='.'){
                            if(are_enemy(piece,target))Targets.push_back(next);
                            break;
                        }
                    }
                }
                b=true;//まだ駒を飛び越えていない
                for(int next=now+W;next<H*W-W;next+=W){
                    char target=board[next];
                    if(b){
                        if(target=='.')Targets.push_back(next);
                        else b=false;
                    }else{
                        if(target!='.'){
                            if(are_enemy(piece,target))Targets.push_back(next);
                            break;
                        }
                    }
                }
                b=true;//まだ駒を飛び越えていない
                for(int next=now-W;next>W;next-=W){
                    char target=board[next];
                    if(b){
                        if(target=='.')Targets.push_back(next);
                        else b=false;
                    }else{
                        if(target!='.'){
                            if(are_enemy(piece,target))Targets.push_back(next);
                            break;
                        }
                    }
                }
            }

            for(const int t:Targets){
                char target=board[t];
                if((target=='.' || are_enemy(target,piece)) && target!='X' ){
                    move_t m={now,t,0};
                    //王が向かい合っていないか確認する
                    if(king_collision(board,m))continue;
                    m.calc_ds(board);
                    ans.push_back(m);
                }
            }
        }//for j end
    }
    return ans;
}


move_t solve(string board){
    vector<move_t> options=f_all_options(board);
    move_t ans={-1,-1,-100};
    vector<move_t>Ans={ans};
    for(move_t m:options){
        //cout<<m.now<<" "<<m.next<<" "<<m.ds<<endl;
        if(ans.ds<m.ds){
            ans=m;
            Ans={ans};
        }else if(ans.ds==m.ds)Ans.push_back(m);
    }
    return Ans[rand()%Ans.size()];
}

void starting(){
    srand((unsigned)time(NULL));
    cout<<fixed<<setprecision(5);

    //駒の価値を設定 大文字は赤
    PieceValue['G']=-100;//将
    PieceValue['A']=-4;//士
    PieceValue['E']=-4;//相
    PieceValue['H']=-8;//馬
    PieceValue['R']=-18;//車
    PieceValue['C']=-9;//砲
    PieceValue['S']=-2;//兵

    PieceValue['g']=100;//将
    PieceValue['a']=4;//士
    PieceValue['e']=4;//相
    PieceValue['h']=8;//馬
    PieceValue['r']=18;//車
    PieceValue['c']=9;//砲
    PieceValue['s']=2;//兵
    //ただし、敵陣の兵卒はプラス２

    cout << "\x1b[43m=================== \x1b[31m 中國象棋 \x1b[39m ===================\x1b[49m \n\n";
    printBoard(library::Initial_board);
    cout<<"中國象棋はxiangqiやシャンチー、Chinese chessとも呼ばれます"<<endl;
    cout<<"中國象棋のルールはこちら：https://ja.wikipedia.org/wiki/シャンチー#ルール\n\n";

    cout<<"あなたが紅(先手)で、CPUが黒(後手)です"<<endl;
    cout<<"あなたの番になったら、動かす駒のx座標、y座標、動かす先のx座標、y座標を順に入力し、エンターキーを押してください"<<endl;
    cout<<"入力には半角数字のみを用い、半角空白で区切ってください"<<endl;

    cout<<"準備ができたら「1 2 3 4」と入力し、エンターキーを押してください\n入力: ";
    int a,b,c,d;
    cin>>a>>b>>c>>d;
    if(a==1 && b==2 && c==3 && d==4)cout<<"では、始めましょう"<<endl;
    else{
        cout<<"一度、ウィンドウを閉じてからやり直してください"<<endl;
        while(true)this_thread::sleep_for(std::chrono::seconds(2));
    }
    return;
}

move_t input(int t,const string &board){
    cout << "次はあなたの番です\n"<<t<<"手目: ";
    int a,b,c,d; cin>>a>>b>>c>>d;
    move_t move={(b+1)*W+a,(d+1)*W+c,0};
    move.calc_ds(board);
    const vector<move_t> V=f_all_options(board);
    //cout<<move.now<<" "<<move.next<<endl;
   // cout<<"合法手の数:"<<V.size()<<endl;
    for(move_t v:V){
        //cout<<v.now<<" "<<v.next<<endl;
        if(v.equal(move))return move;
    }
    move.now=-1;
    return move;
}


int main() {
    starting();

    string board=library::Initial_board;
    int t=1;
    while (t<250) {
        auto start = std::chrono::high_resolution_clock::now();
        move_t human=input(t,board);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        cout<<"思考時間："<<elapsed_seconds.count()<<"秒\n";
        if(human.now==-1){
            cout<<"不正な手です\n"<<library::YOU_LOSE<<endl;
            return 0;
        }
        if(human.ds<-50){//敵将の首を取った！
            cout<<library::YOU_WIN<<endl;
            return 0;
        }
        board=move_a_piece(human,board);
        printBoard(board);
        t++;
        
        cout<<"次はCPUの番です\n"<<t<<"手目: ";
        double time_start=clock();
        move_t CPU=solve(board);
        if(CPU.now==-1){
            cout<<"投了\n";
            cout<<"思考時間："<<(clock()-time_start)/CLOCKS_PER_SEC<<"秒\n";
            cout<<library::YOU_WIN<<endl;
            return 0;
        }
        cout<<CPU.now%W<<' '<<CPU.now/W-1<<' '<<CPU.next%W<<' '<<CPU.next/W-1<<endl;
        cout<<"思考時間："<<(clock()-time_start)/CLOCKS_PER_SEC<<"秒\n";
        if(CPU.ds>50){
            cout<<library::YOU_LOSE<<endl;
            return 0;
        }
        board=move_a_piece(CPU,board);
        printBoard(board);
        t++;
    }

    return 0;
}