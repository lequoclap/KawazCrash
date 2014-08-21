//
//  MainScene.cpp
//  KawazCrash
//
//  Created by giginet on 6/19/14.
//
//

#include "MainScene.h"
#include "Cookie.h"
#include <algorithm>
#include "Cocostudio/cocostudio.h"

#include "cookie_main.h"
#include "cookie_crush_acf.h"


#include "ADX2Manager.h"

USING_NS_CC;

const int HORIZONTAL_COUNT = 6;
const int VERTICAL_COUNT = 8;
const int VANISH_COUNT = 4;
/// Stage用のNodeのタグ
const int STAGE_TAG = 50000;

MainScene::MainScene()
: _state(State::Ready)
,_second(60)
,_score(0)
,_comboCount(0)
,_stage(nullptr)
,_currentCookie(nullptr)
,_cue(nullptr)
,_scoreLabel(nullptr)
,_secondLabel(nullptr)
{
    // ADX2を初期化します
    CriAtomExStandardVoicePoolConfig vp_config;
    criAtomExVoicePool_SetDefaultConfigForStandardVoicePool(&vp_config);
    vp_config.num_voices = 8;
    vp_config.player_config.streaming_flag = CRI_TRUE;
    vp_config.player_config.max_sampling_rate = 48000 << 1;
    
    CriAtomExPlayerConfig pf_config;
    criAtomExPlayer_SetDefaultConfig(&pf_config);
    pf_config.max_path_strings = 1;
    pf_config.max_path = 256;
    
    ADX2::ADX2Manager::initialize(pf_config, vp_config);
}

MainScene::~MainScene()
{
    CC_SAFE_RELEASE_NULL(_stage);
    CC_SAFE_RELEASE_NULL(_currentCookie);
    CC_SAFE_RELEASE_NULL(_cue);
    CC_SAFE_RELEASE_NULL(_scoreLabel);
    CC_SAFE_RELEASE_NULL(_secondLabel);
    // ADX2を終了します
    ADX2::ADX2Manager::finalize();
}

Scene* MainScene::createScene()
{
    auto scene = Scene::create();
    auto main = MainScene::create();
    scene->addChild(main);
    return scene;
}

bool MainScene::init()
{
    if (!Layer::init()) {
        return false;
    }
    
    // 画面サイズの取得
    auto winSize = Director::getInstance()->getWinSize();
    
    // シーンファイルを読み込み
    auto node = cocostudio::SceneReader::getInstance()->createNodeWithSceneFile("MainScene.json");
    // シーンファイルの高さ
    const auto sceneHeight = 568;
    // 3.5インチ環境で下の部分を隠すために画面の高さによって位置を変えている
    node->setPosition(Vec2(0, -(sceneHeight - winSize.height)));
    this->addChild(node);
    
    auto cue = ADX2::Cue::create("adx2/cookie/cookie_crush.acf", "adx2/cookie/cookie_main.acb");
    this->setCue(cue);
    
    this->setStage(Node::create());
    auto stageSize = Size(Cookie::getSize() * HORIZONTAL_COUNT, Cookie::getSize() * VERTICAL_COUNT);
    _stage->setContentSize(stageSize);
    auto layer = LayerColor::create(Color4B::RED, stageSize.width, stageSize.height);
    _stage->addChild(layer);
    
    for (int x = 0; x < HORIZONTAL_COUNT; ++x) {
        for (int y = 0; y < VERTICAL_COUNT; ++y) {
            auto cookie = Cookie::create();
            cookie->setCookiePosition(Vec2(x, y));
            this->addCookie(cookie);
        }
    }
    
    auto stage = node->getChildByTag(STAGE_TAG);
    stage->addChild(_stage, 1);
    
    
    // タッチイベントの登録
    auto listener = EventListenerTouchOneByOne::create();
    listener->onTouchBegan = [this](Touch* touch, Event* event) {
        auto position = touch->getLocation();
        // 現在のタッチ位置にあるクッキーを取り出す
        auto cookie = this->getCookieAtByWorld(position);
        
        // 現在移動中のクッキーとして設定
        this->setCurrentCookie(cookie);
        return true;
    };
    listener->onTouchMoved = [this](Touch* touch, Event* event) {
        // 移動先のクッキーを取得
        auto nextCookie = this->getCookieAtByWorld(touch->getLocation());
        
        // Main以外では何もしない
        if (_state != State::Main) return;
        
        // 元のクッキー、移動後のクッキーが共に存在していて、違った場合、移動させる
        if (_currentCookie != nullptr &&
            nextCookie != nullptr &&
            _currentCookie != nextCookie) {
            
            // 現在の選択しているクッキーと、移動先のクッキーが共に動いていない状態であるとき
            if (_currentCookie->isStatic() && nextCookie->isStatic()) {
                auto cp = _currentCookie->getCookiePosition();
                auto np = nextCookie->getCookiePosition();
                // 移動先のクッキーが右方向にあるとき
                if (cp.y == np.y && cp.x + 1 == np.x) {
                    // 2枚のクッキーを入れ替える
                    this->swapCookies(_currentCookie, nextCookie);
                    
                    // 現在選択中のクッキーを外す
                    this->setCurrentCookie(nullptr);
                }
                if (cp.y == np.y && cp.x - 1 == np.x) { // 左方向
                    this->swapCookies(_currentCookie, nextCookie);
                    this->setCurrentCookie(nullptr);
                }
                if (cp.x == np.x && cp.y + 1 == np.y) { // 上方向
                    this->swapCookies(_currentCookie, nextCookie);
                    this->setCurrentCookie(nullptr);
                }
                if (cp.x == np.x && cp.y - 1 == np.y) { // 下方向
                    this->swapCookies(_currentCookie, nextCookie);
                    this->setCurrentCookie(nullptr);
                }
            }
        }
    };
    listener->onTouchEnded = [this](Touch* touch, Event* event) {
        // 現在選択中のクッキーを外す
        this->setCurrentCookie(nullptr);
    };
    listener->onTouchCancelled = [this](Touch* touch, Event* event) {
        // 現在選択中のクッキーを外す
        this->setCurrentCookie(nullptr);
    };
    this->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);
    this->scheduleUpdate();
    
    auto secondLabel = node->getChildByTag(50002)->getChildren().at(0)->getChildByTag(6);
    this->setSecondLabel(dynamic_cast<ui::TextAtlas *>(secondLabel));
    
    auto scoreLabel = node->getChildByTag(50003)->getChildren().at(0)->getChildByTag(6);
    this->setScoreLabel(dynamic_cast<ui::TextAtlas *>(scoreLabel));
    
    return true;
}

void MainScene::onEnterTransitionDidFinish()
{
    Layer::onEnterTransitionDidFinish();
    _cue->playCueByID(CRI_COOKIE_MAIN_BGM);
    
    auto gamestart = Sprite::create("gamestart.png");
    auto winSize = Director::getInstance()->getWinSize();
    gamestart->setPosition(Vec2(winSize.width / 2.0, winSize.height / 1.5));
    gamestart->setScale(0);
    gamestart->runAction(Sequence::create(EaseElasticIn::create(ScaleTo::create(0.5, 1.0)),
                                          DelayTime::create(1.5),
                                          MoveBy::create(0.5, Vec2(0, winSize.height / 2.0)),
                                          RemoveSelf::create(),
                                          NULL));
    this->addChild(gamestart, 2);
    setState(State::Main);
}

void MainScene::update(float dt)
{
    // ADX2を更新する
    ADX2::ADX2Manager::getInstance()->update();
    
    // 全クッキーに対して落下を判定する
    for (auto cookie : _cookies) {
        this->fallCookie(cookie);
    }
    
    if (_state == State::Main) {
        
        // フィールドの更新
        this->updateField();
        
        // スコアの更新
        _scoreLabel->setString(StringUtils::toString((int)_score));
        
        // 残り時間の更新
        _second -= dt;
        _secondLabel->setString(StringUtils::toString((int)_second));
        
        if (_second <= 0) {
            setState(State::Result);
            auto gamestart = Sprite::create("timeup.png");
            auto winSize = Director::getInstance()->getWinSize();
            gamestart->setPosition(Vec2(winSize.width / 2.0, winSize.height / 1.5));
            gamestart->setScale(0);
            gamestart->runAction(Sequence::create(EaseElasticIn::create(ScaleTo::create(0.5, 1.0)),
                                                  DelayTime::create(1.5),
                                                  ScaleTo::create(0.5, 0),
                                                  RemoveSelf::create(),
                                                  NULL));
            this->addChild(gamestart, 2);
        }
    }
}

Cookie* MainScene::getCookieAt(cocos2d::Vec2 position)
{
    for (auto& cookie : _cookies) {
        if (cookie->getCookiePosition().x == position.x &&
            cookie->getCookiePosition().y == position.y) {
            return cookie;
        }
    }
    return nullptr;
}

Cookie* MainScene::getCookieAtByWorld(cocos2d::Vec2 worldPosition)
{
    auto stagePoint = _stage->convertToNodeSpace(worldPosition);
    auto x = floor(stagePoint.x / Cookie::getSize());
    auto y = floor(stagePoint.y / Cookie::getSize());
    return this->getCookieAt(Vec2(x, y));
}

void MainScene::addCookie(Cookie *cookie)
{
    _cookies.pushBack(cookie);
    _stage->addChild(cookie);
    cookie->adjustPosition();
}

void MainScene::moveCookie(Cookie *cookie, cocos2d::Vec2 cookiePosition)
{
    cookie->setCookiePosition(cookiePosition);
    cookie->adjustPosition();
}

void MainScene::swapCookies(Cookie *cookie0, Cookie *cookie1)
{
    // 連鎖数を0にする
    _comboCount = 0;
    
    // 効果音を鳴らす
    _cue->playCueByID(CRI_COOKIE_MAIN_SWIPE);
    
    // 画面上の位置を取得しておく
    auto position0 = cookie0->getPosition();
    auto position1 = cookie1->getPosition();
    
    // フィールド上の位置も取得しておく
    auto cookiePosition0 = cookie0->getCookiePosition();
    auto cookiePosition1 = cookie1->getCookiePosition();
    
    // 消去判定のため、予め動かす
    cookie1->setCookiePosition(cookiePosition0);
    cookie0->setCookiePosition(cookiePosition1);
    
    // それぞれ、動いた後の位置で隣接しているクッキーを取り出す
    auto cookies0 = this->checkNeighborCookies(cookie0);
    auto cookies1 = this->checkNeighborCookies(cookie1);
    
    // いずれかが4つ以上繋がってる場合、移動可能
    bool canMove = cookies0.size() >= VANISH_COUNT || cookies1.size() >= VANISH_COUNT;
    
    // 移動中状態にする
    cookie0->setState(Cookie::State::SWAPPING);
    cookie1->setState(Cookie::State::SWAPPING);
    
    // 元の位置に戻しておく
    cookie1->setCookiePosition(cookiePosition1);
    cookie0->setCookiePosition(cookiePosition0);
    
    // 移動アニメーションの追加関数
    auto addMoveAnimation = [canMove, this](Cookie *cookie, Vec2 fromPosition, Vec2 toPosition, Vec2 toCookiePosition)
    {
        const auto duration = 0.2;
        
        cookie->runAction(Sequence::create(MoveTo::create(duration, toPosition),
                                           CallFuncN::create([=](Node *node) {
            auto cookie = dynamic_cast<Cookie *>(node);
            // もし、クッキーが動かせるとき
            if (canMove) {
                // 位置を入れ替える
                this->moveCookie(cookie, toCookiePosition);
                
                // 状態を静止中にする
                cookie->setState(Cookie::State::STATIC);
            } else {
                // もし、動かせないとき
                // 元に戻すアニメーション
                cookie->runAction(Sequence::create(MoveTo::create(duration / 2.0, fromPosition),
                                                   CallFuncN::create([] (Node *node) {
                    auto cookie = dynamic_cast<Cookie *>(node);
                    cookie->setState(Cookie::State::STATIC);
                }), NULL));
            }
        }), NULL));
    };
    
    // 移動アニメーションを追加する
    addMoveAnimation(cookie0, position0, position1, cookiePosition1);
    addMoveAnimation(cookie1, position1, position0, cookiePosition0);
}

bool MainScene::vanishCookies(CookieVector cookies)
{
    // 全てが消去可能かを調べる
    bool canVanish = std::all_of(cookies.begin(),
                                 cookies.end(),
                                 [](Cookie *cookie) {
                                     return cookie->isStatic();
                                 });
    if (cookies.size() >= VANISH_COUNT && canVanish) {
        for (auto cookie : cookies) {
            this->deleteCookie(cookie);
        }
        return true;
    }
    return false;
}

void MainScene::deleteCookie(Cookie *cookie)
{
    if (!cookie) return;
    cookie->setState(Cookie::State::DISAPEARING);
    auto duration = 0.2f;
    cookie->runAction(Sequence::create(FadeOut::create(duration),
                                       CallFuncN::create([this](Node* node) {
        auto cookie = dynamic_cast<Cookie *>(node);
        _cookies.eraseObject(cookie);
    }),
                                       RemoveSelf::create(),
                                       NULL));
}

CookieVector MainScene::checkNeighborCookies(Cookie *cookie) {
    // 空のベクターを作って渡している
    CookieVector v;
    return this->checkNeighborCookies(cookie, std::move(v));
}

CookieVector MainScene::checkNeighborCookies(Cookie *cookie, CookieVector checked) {
    // すでにチェック済みだった場合は何もせずにそのままチェック済みのベクターを返す
    if (checked.contains(cookie)) {
        return std::move(checked);
    }
    // クッキーをチェック済み一覧に追加する
    checked.pushBack(cookie);
    
    // チェックするクッキーのグリッド上の位置を取り出す
    auto position = cookie->getCookiePosition();
    
    // グリッド上の上下左右にあるクッキーを取得する
    auto up = this->getCookieAt(Vec2(position.x, position.y + 1));
    auto down = this->getCookieAt(Vec2(position.x, position.y - 1));
    auto left = this->getCookieAt(Vec2(position.x - 1, position.y));
    auto right = this->getCookieAt(Vec2(position.x + 1, position.y));
    
    // 上にクッキーがあって同じ形なら、上にあるクッキーも再帰的にチェックする
    if (up && up->getCookieShape() == cookie->getCookieShape()) {
        checked = this->checkNeighborCookies(up, std::move(checked));
    }
    // 下
    if (down && down->getCookieShape() == cookie->getCookieShape()) {
        checked = this->checkNeighborCookies(down, std::move(checked));
    }
    // 左
    if (left && left->getCookieShape() == cookie->getCookieShape()) {
        checked = this->checkNeighborCookies(left, std::move(checked));
    }
    // 右
    if (right && right->getCookieShape() == cookie->getCookieShape()) {
        checked = this->checkNeighborCookies(right, std::move(checked));
    }
    return std::move(checked);
}

bool MainScene::fallCookie(Cookie *cookie)
{
    auto position = cookie->getCookiePosition();
    // すでに一番下にあったとき、落ちない
    if (position.y == 0) {
        return false;
    }
    // クッキーがSTATIC状態じゃなかったとき、落ちない
    if (!cookie->isStatic()) {
        return false;
    }
    auto downPosition = Vec2(position.x, position.y - 1);
    // 1つ下のクッキーを取り出す
    auto down = this->getCookieAt(Vec2(position.x, position.y - 1));
    // 1つ下がなかったとき、落ちる
    if (down == nullptr) {
        auto duration = 0.05;
        cookie->setState(Cookie::State::FALLING);
        cookie->runAction(Sequence::create(MoveBy::create(duration, Vec2(0, -Cookie::getSize())),
                                           CallFuncN::create([this, downPosition] (Node *node) {
            // 落下アニメーション終了語
            auto cookie = dynamic_cast<Cookie *>(node);
            // クッキーを動かす
            this->moveCookie(cookie, downPosition);
            cookie->setState(Cookie::State::STATIC);
            // さらに落ちないか再度落下判定を行う
            this->fallCookie(cookie);
        }),
                                           NULL));
        return true;
    }
    return false;
}

cocos2d::Vector<Cookie *> MainScene::spawnCookies()
{
    cocos2d::Vector<Cookie *> cookies;
    // 一番上の座標を取り出す
    auto y = VERTICAL_COUNT - 1;
    // 一番上の行を全てチェック
    for (int x = 0; x < HORIZONTAL_COUNT; ++x) {
        auto cookie = this->getCookieAt(Vec2(x, y));
        if (!cookie) { // もしクッキーがなければ
            // クッキーを追加する
            auto cookie = Cookie::create();
            cookie->setCookiePosition(Vec2(x, y));
            this->addCookie(cookie);
        }
    }
    return std::move(cookies);
}

bool MainScene::canVanishNext(Cookie *cookie)
{
    auto cookies = this->checkNeighborCookies(cookie);
    auto skews = std::vector<Vec2> {Vec2(1, 1), Vec2(1, -1), Vec2(-1, 1), Vec2(-1, -1)};
    auto allDirections = std::vector<Vec2> {Vec2(0, 2), Vec2(2, 0), Vec2(0, -2), Vec2(-2, 0), Vec2(1, 1), Vec2(1, -1), Vec2(-1, 1), Vec2(-1, -1)};
    auto currentVector = cookie->getCookiePosition();
    
    if (cookies.size() >= VANISH_COUNT) {
        // 4以上は存在しないはずだけどtrue
        return true;
    } else if (cookies.size() == 3) {
        // もし3つ繋がっていたとき
        for (auto vector : allDirections) {
            auto nextVector = cookie->getCookiePosition() + vector;
            auto nextCookie = this->getCookieAt(nextVector);
            // 塊に含まれるクッキーそれぞれの距離2の位置に、別の同種のクッキーが1つでもあったら消せる
            if (nextCookie && cookie && nextCookie->getCookieShape() == cookie->getCookieShape() && !cookies.contains(nextCookie)) {
                return true;
            }
        }
    } else if (cookies.size() == 2) {
        // もし2つ繋がっていたとき
        for (auto vector : allDirections) {
            auto nextVector = cookie->getCookiePosition() + vector;
            auto nextCookie = this->getCookieAt(nextVector);
            // 塊に含まれるクッキーの距離2の位置に、別の同種のクッキーがある
            if (nextCookie && cookie && nextCookie->getCookieShape() == cookie->getCookieShape() && !cookies.contains(nextCookie)) {
                // かつ、そのクッキーと塊のクッキーがナナメ方向に共通の同種のクッキーを持っている
                for (auto sv0 : skews) {
                    for (auto sv1 : skews) {
                        auto e0 = this->getCookieAt(currentVector + sv0);
                        auto e1 = this->getCookieAt(nextVector + sv1);
                        if (e0 &&
                            e1 &&
                            e0->getCookieShape() == cookie->getCookieShape() &&
                            e0 == e1 &&
                            !cookies.contains(e0) &&
                            !cookies.contains(e1)) {
                            // 斜めに共通のcookieがあれば消せる
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void MainScene::updateField()
{
    // クッキーの生成
    this->spawnCookies();
    
    if (this->isAllStatic()) {
        
        // 既に揃ってるのを消す
        auto vanished = false;
        CookieVector checked;
        for (int x = 0; x < HORIZONTAL_COUNT; ++x) {
            for (int y = 0; y < VERTICAL_COUNT; ++y) {
                auto cookie = this->getCookieAt(Vec2(x, y));
                if (cookie && !checked.contains(cookie)) {
                    CookieVector v = this->checkNeighborCookies(cookie);
                    if (v.size() >= VANISH_COUNT && !vanished) {
                        _comboCount += 1;
                        vanished = true;
                        // スコアの追加
                        _score += 1000 * pow(3, _comboCount);
                        // コンボカウンターを表示
                        this->showChainCount(v.getRandomObject(), _comboCount);
                    }
                    checked.pushBack(v);
                    this->vanishCookies(std::move(v));
                }
            }
        }
        if (vanished) {
            float gameVariable = _comboCount * 0.125;
            gameVariable = MIN(1.0, gameVariable);
            criAtomEx_SetGameVariableByName("ComboCount", gameVariable);
            _cue->playCueByID(CRI_COOKIE_MAIN_VANISH);
        }
        
        // 次にどれも消えなさそうだったらランダムに2列消す
        for (int x = 0; x < HORIZONTAL_COUNT; ++x) {
            for (int y = 0; y < VERTICAL_COUNT; ++y) {
                auto cookie = this->getCookieAt(Vec2(x, y));
                if (cookie && this->canVanishNext(cookie)) {
                    // どれか消えそうなら探索を打ち切る
                    return;
                }
            }
        }
        // もしどれも消えなかったとき、ランダムに2列を選んで消去する
        auto baseX = rand() % HORIZONTAL_COUNT;
        auto otherX = (rand() % (HORIZONTAL_COUNT - 1) + baseX) % HORIZONTAL_COUNT;
        for (int y = 0; y < VERTICAL_COUNT; ++y) {
            this->deleteCookie(this->getCookieAt(Vec2(baseX, y)));
            this->deleteCookie(this->getCookieAt(Vec2(otherX, y)));
        }
    }
}

bool MainScene::isAllStatic()
{
    return std::all_of(_cookies.begin(),
                       _cookies.end(),
                       [](Cookie* cookie) { return cookie->isStatic(); });
}

void MainScene::showChainCount(Cookie * cookie, int comboCount)
{
    std::string filename;
    if (comboCount >= 8) {
        // コンボ数が8以上なら色を変える
        filename = "Chain1_1.json";
    } else {
        filename = "Chain0_1.json";
    }
    auto chainCount = cocostudio::GUIReader::getInstance()->widgetFromJsonFile(filename.c_str());
    chainCount->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    ui::TextAtlas * atlas = dynamic_cast<ui::TextAtlas *>(chainCount->getChildByTag(8));
    atlas->setString(StringUtils::toString(comboCount));
    chainCount->setPosition(cookie->getParent()->convertToWorldSpace(cookie->getPosition()));
    chainCount->setScale(0);
    this->addChild(chainCount, 1000);
    
    // ToDo アニメーションはcocoStudio側で持たせた方が綺麗かも！
    chainCount->runAction(Sequence::create(ScaleTo::create(0.2, 1.0),
                                           DelayTime::create(1.0),
                                           ScaleTo::create(0.2, 0),
                                           RemoveSelf::create(), NULL));
    
}