// UTF-8

#include "include/dxlibp.h"
#include "include/exception.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <fastmath.h>
#include <time.h>

#include <pspmath.h>

PSP_MODULE_INFO("Particle", 0, 1, 1)
;
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

/*
 HSV to RGB

 http://laputa.cs.shinshu-u.ac.jp/~gtakano/prog3.html
 こちらのページより拝借させて頂きました。
 ライセンスに当たるものが見つからなかったので無断で使用及び改変をさせて頂きました。
 もしライセンスに当たる物を見つけたなど、問題があるようならご一報下さい。
 */
unsigned int HSV2RGB(float H, float S, float V)
{
	int in = (int) floor(H / 60);
	float fl = (H / 60) - in;
	if (!(in & 1))
		fl = 1 - fl; // if i is even

	float m = V * (1 - S);
	float n = V * (1 - S * fl);

	float r, g, b;
	switch (in)
	{
	case 0:
		r = V, g = n, b = m;
		break;
	case 1:
		r = n, g = V, b = m;
		break;
	case 2:
		r = m, g = V, b = n;
		break;
	case 3:
		r = m, g = n, b = V;
		break;
	case 4:
		r = n, g = m, b = V;
		break;
	case 5:
		r = V, g = m, b = n;
		break;
	default:
		r = 0, g = 0, b = 0;
		break;
	}
	return (unsigned int) (0xFF << 24 | // alfa
		(unsigned char) (b * 0xFF) << 16 | // 16-23
		(unsigned char) (g * 0xFF) << 8 | // 8-15
		(unsigned char) (r * 0xFF)); // 0-7
}

float GetRandSafe()
{
	return (GetRand(98) + 1) / 10.0f;
}

////////////////////////////////////////////

#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>

namespace Psp
{
// 突貫のアロケータ（ｇｄｇｄ）
template<typename _Tp>
class allocator : public std::allocator<_Tp>
{
public:
	typedef size_t     size_type;
	typedef ptrdiff_t  difference_type;
	typedef _Tp*       pointer;
	typedef const _Tp* const_pointer;
	typedef _Tp&       reference;
	typedef const _Tp& const_reference;
	typedef _Tp        value_type;

private:
	static std::map<_Tp*, SceUID> indexs;

public:
	template<typename _Tp1>
		struct rebind
		{ typedef allocator<_Tp1> other; };

	allocator(){}

	allocator(const allocator& __a)
		: std::allocator<_Tp>(__a){}

	template<typename _Tp1>
	allocator(const allocator<_Tp1>&){}

	~allocator() { }

	pointer
	allocate(size_type __n, const void* = 0)
	{
	if (__builtin_expect(__n > this->max_size(), false))
	  std::__throw_bad_alloc();

	SceUID id = sceKernelAllocPartitionMemory(2, "StlMem", 0, __n * sizeof(_Tp), NULL);
	_Tp* ptr = static_cast<_Tp*>(sceKernelGetBlockHeadAddr(id));

	if(!ptr || !id)
		std::__throw_bad_alloc();

	indexs.insert(std::pair<_Tp*, SceUID>(ptr, id));

	return ptr;
	}

	// __p is not permitted to be a null pointer.
	void
	deallocate(pointer __p, size_type)
	{
		SceUID id = indexs[__p];
		sceKernelFreePartitionMemory(id);
		indexs.erase(__p);
	}

	size_type
	max_size()
	{
		return sceKernelMaxFreeMemSize() / sizeof(_Tp);
	}
};

template<typename _Tp>
std::map<_Tp*, SceUID> allocator<_Tp>::indexs;

}

#ifdef _PSP
using Psp::allocator;
#else
using std::allocator;
#endif

class BlockBreaker;
class Screen
{
	static uint32_t m_width, m_height;

public:
	Screen(void)
	{
	}

	Screen(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
	}

public:
	uint32_t GetWidth(void) const
	{
		return m_width;
	}

	uint32_t GetHeight(void) const
	{
		return m_height;
	}
};

uint32_t Screen::m_width, Screen::m_height;

class Particle
{
	friend class BlockBreaker;
	friend class Blocks;

	float m_x, m_y;
	float m_vx, m_vy;
	uint32_t m_color;

	enum State
	{
		BLOCK, FALL, BALL, DELETED
	} m_state;

public:
	Particle(uint16_t x = 200, uint16_t y = 200) :
		m_x(x), m_y(y), m_state(BLOCK)
	{
	}
};
class Bar
{
	const Screen m_screen;

	uint16_t m_x, m_y;
	const uint16_t m_width, m_height;
	const uint32_t m_color;

	const uint32_t m_speed;
	const uint8_t m_rate;
	bool m_speedup;

public:
	// メンバイニシャライザ
	Bar(uint16_t width, uint16_t height, uint32_t color, uint32_t speed,
		uint8_t rate = 3) :
			m_x(m_screen.GetWidth() / 2), // 中央
			m_y(m_screen.GetHeight() - height), // 下から少し
			m_width(width), m_height(height), m_color(color), m_speed(speed),
			m_rate(rate)
	{
	}

	// もどき
	bool IsHited(uint32_t x, uint32_t y)
	{
		// 条件式の簡略化
		// 0-1=0xFF..Fで > m_widthとなる
		return x - m_x < m_width && y - m_y < m_height;

	}

	bool SetSpeedUpFlag(bool arg)
	{
		return m_speedup = arg;
	}

	void MoveRight(void)
	{
		uint8_t rate = m_speedup ? m_rate : 1;

		if (m_x < m_screen.GetWidth() - m_width - m_speed * rate)
			m_x += m_speed * rate;
		else
			m_x = m_screen.GetWidth() - m_width;
	}

	void MoveLeft(void)
	{
		uint8_t rate = m_speedup ? m_rate : 1;

		if (m_x >= m_speed * rate)
			m_x -= m_speed * rate;
		else
			m_x = 0;
	}

	void Update(void)
	{
		// バーの移動
		if (GetInputState() & (DXP_INPUT_LTRIGGER | DXP_INPUT_RTRIGGER))
			SetSpeedUpFlag(1);
		else
			SetSpeedUpFlag(0);

		if (GetInputState() & DXP_INPUT_RIGHT)
			MoveRight();
		else if (GetInputState() & DXP_INPUT_LEFT)
			MoveLeft();

		DrawBox(m_x, m_y, m_x + m_width, m_y + m_height, 0xFF00FF00, 1);
	}
};
class Blocks
{
	uint16_t m_width;
	uint16_t m_height;

	typedef std::vector<Particle, allocator<Particle> > vector;
	vector m_blocks;

public:
	Blocks(uint16_t height) :
		m_width(Screen().GetWidth()), m_height(height), //
			m_blocks(m_width * m_height + 1)
	{
		for (int i = 0; i < m_width; i++)
		{
			// make a color
			uint32_t c = HSV2RGB((float) i * (360.0f / m_width), 1, 1);
			for (int j = 0; j < m_height; j++)
			{
				Particle& p = m_blocks[i + j * m_width];
				p.m_x = i;
				p.m_y = j;
				p.m_color = c;
			}
		}
	}

public:
	vector& GetBlocks()
	{
		return m_blocks;
	}

	Particle* GetParticle(uint32_t x, uint32_t y)
	{
		uint32_t index = x + y * m_width;
		if (index >= m_blocks.size())
		{
			return NULL;
		}
		return &m_blocks[index];
	}
};
class BlockBreaker
{
	class Worker;
	class Counter
	{
		friend class Worker;

		BlockBreaker& m_breaker;
		uint32_t m_block, m_fall, m_ball;

	public:
		Counter(BlockBreaker& breaker) :
			m_breaker(breaker), //
				m_block(m_breaker.m_blocks.GetBlocks().size()), //
				m_fall(0), //
				m_ball(0) //
		{
		}

	public:
		uint32_t GetBlockCount(void)
		{
			return this->m_block;
		}
		uint32_t GetFallCount(void)
		{
			return this->m_fall;
		}
		uint32_t GetBallCount(void)
		{
			return this->m_ball;
		}
		uint32_t GetRemovedBallCount(void)
		{
			return m_breaker.m_blocks.GetBlocks().size() - //
				GetBlockCount() - //
				GetFallCount() - //
				GetBallCount();
		}

	};

	const Screen m_screen;
	Blocks& m_blocks;
	Bar& m_bar;
	Counter m_counter;

	bool m_cleared;
	//int m_graph_handler;

public:
	BlockBreaker(Bar& bar, Blocks& blocks) :
		m_blocks(blocks), m_bar(bar), m_counter(*this), m_cleared(false)
	{
		Worker worker(*this);
		// trigger to start
		Particle& particle = m_blocks.GetBlocks().back();
		worker.BreakParticle(particle);
		worker.MakeBall(particle);
		particle.m_x = m_screen.GetWidth() / 2;
		particle.m_y = m_screen.GetHeight() / 2;
		//particle.m_x = 1; // さっさと「ドバーッ」モードにしたい人向け
		//particle.m_y = 1;
		particle.m_vx = GetRandSafe();
		particle.m_vy = -GetRandSafe();
		particle.m_color = 0xFFFFFFFF;
	}

private:
	class Worker: std::unary_function<Particle&, void>
	{
		BlockBreaker& m_breaker;

	public:
		Worker(BlockBreaker& breaker) :
			m_breaker(breaker)
		{
		}

	public:
		bool RemoveParticle(Particle& particle)
		{
			bool i = true;
			if (particle.m_state == particle.FALL)
				m_breaker.m_counter.m_fall -= 1;
			else if (particle.m_state == particle.BALL)
				m_breaker.m_counter.m_ball -= 1;
			else
				i = false;

			particle.m_state = particle.DELETED;
			return i;
		}
		void BreakParticle(Particle& particle)
		{
			if (particle.m_state == particle.BLOCK)
				m_breaker.m_counter.m_block -= 1;

			particle.m_state = particle.FALL;
			m_breaker.m_counter.m_fall += 1;
		}
		void MakeBall(Particle& particle)
		{
			if (particle.m_state == particle.FALL)
				m_breaker.m_counter.m_fall -= 1;

			particle.m_state = particle.BALL;
				m_breaker.m_counter.m_ball += 1;
		}

	private:
		void UpdateBlock(Particle& particle)
		{
			DrawPixel(particle.m_x, particle.m_y, particle.m_color);
		}

		void UpdateFallBlock(Particle& particle)
		{
			particle.m_vy += 0.1;
			particle.m_x += particle.m_vx;
			particle.m_y += particle.m_vy;

			DrawPixel(particle.m_x, particle.m_y, particle.m_color);

			if (m_breaker.m_bar.IsHited(particle.m_x, particle.m_y))
			{
				MakeBall(particle);

				//	newball.vx = GetRandSafe() * (particle.m_vx < 0?-1:1); //prototype
				particle.m_vx = GetRandSafe(); //simple
				particle.m_vy = GetRandSafe() + 1;
			}
			else if (particle.m_y > m_breaker.m_screen.GetHeight())
			{
				RemoveParticle(particle);
			}
		}

		void UpdateBall(Particle& particle)
		{
			float bvx = particle.m_vx;
			float bvy = particle.m_vy;
			float bspeed = vfpu_powf(0.5f, bvx * bvx + bvy * bvy);
			float bradius = vfpu_atan2f(bvy, bvx);

			for (int i = 0; i < bspeed; i++)
			{
				particle.m_x += particle.m_vx / bspeed;
				particle.m_y += particle.m_vy / bspeed;

				Particle* hitParticle = //
					m_breaker.m_blocks.GetParticle( //
						particle.m_x, particle.m_y);

			//	printfDx("ball:%p (%f,%f)\n",hitParticle,particle.m_x, particle.m_y);

				if (hitParticle)
					if (hitParticle->m_state == hitParticle->BLOCK)
					{
					//	printfDx("hit:%p (%f,%f)\n", hitParticle, particle.m_x, particle.m_y);

						BreakParticle(*hitParticle);

						// すこし弄らせてもらいましたサーセンｗ
						hitParticle->m_vx = //
							vfpu_cosf(bradius + M_PI * 2 / //
								(3 * GetRandSafe()) - 15) * //
								((hitParticle->m_vx < 0) ? -3 : 3);
						hitParticle->m_vy = 1;

						// reflection
						particle.m_vy = -particle.m_vy;
					}

				//この辺は純粋にノータッチ
				if ((particle.m_x < 0 && particle.m_vx < 0) //
					|| (particle.m_x > m_breaker.m_screen.GetWidth() //
						&& particle.m_vx > 0))
				{
					particle.m_vx = -particle.m_vx;
				}
				if (particle.m_y < 0 && particle.m_vy < 0)
				{
					particle.m_vy = -particle.m_vy;
				}
				if (particle.m_y > m_breaker.m_screen.GetHeight())
				{
					RemoveParticle(particle);
				}
				if (m_breaker.m_bar.IsHited(particle.m_x, particle.m_y))
				{
					particle.m_vy = -fabsf(particle.m_vy);
				}
				DrawPixel(particle.m_x, particle.m_y, particle.m_color);
			}
		}

	public:
		void operator()(Particle& particle)
		{
			switch (particle.m_state)
			{
			case (Particle::DELETED):
				return;

			case (Particle::BLOCK):
				UpdateBlock(particle);
				break;

			case (Particle::FALL):
				UpdateFallBlock(particle);
				break;

			case (Particle::BALL):
				UpdateBall(particle);
				break;

			default:
				break;
			}
		}
	};

public:
	void Update(void)
	{
		// 走査。<algorithm>愛してる。
		std::for_each( //
			m_blocks.GetBlocks().begin(), // 先頭
			m_blocks.GetBlocks().end(), // 後尾
			Worker(*this)); // ファンクタ

		m_bar.Update();

		if (m_counter.GetBlockCount() == 0)
		{
			/*
			// クリアー！
			DrawString(
				m_screen.GetWidth() / 2 - 25, // 25は適当
				m_screen.GetHeight() / 2 - 16, // FONTHEIGHT
				"CLEAR!",//
				DXP_COLOR_WHITE);

			DrawString(
				m_screen.GetWidth() / 2 - 25, // 適当
				m_screen.GetHeight() / 2, // 中央
				"\x82\xA8\x82\xDF\x82\xC5\x82\xC6", // おめでと
				DXP_COLOR_WHITE);
			*/

			clsDx();

			// クリアー！
			for(int i = 0; i < 9; i++)
				printfDx("\n");
			for(int i = 0; i < 37; i++)
				printfDx(" ");
			printfDx("CLEAR!\n");
			for(int i = 0; i < 36; i++)
				printfDx(" ");
			printfDx("\x82\xA8\x82\xDF\x82\xC5\x82\xC6\n");

			//m_graph_handler = MakeGraph(480,272);

			m_cleared = 1;
		}
	}

public:
	// 使わなくなｔｔ＼（＾o＾）／
	/*
	// クリア前の動作は未定義
	void Redraw()
	{
		DrawGraph(0, 0, m_graph_handler, 0);
	}
	*/

public:
	bool IsCleared(void) const
	{
		return m_cleared;
	}
	Counter& GetCounter(void)
	{
		return m_counter;
	}
};

class Fps
{
	int m_count;
	int m_time;
	int m_value;

public:
	Fps() :
		m_count(0), m_time(GetNowCount()), m_value(0)
	{
	}

	int Update()
	{
		m_count++;

		// 一秒たったか
		if (m_time + 1000 <= GetNowCount())
		{
			m_value = m_count;
			m_count = 0;
			m_time = GetNowCount();
		}
		return m_count;
	}

	int GetFps()
	{
		return m_value;
	}
};

// ロック付きスイッチ
bool CheckKey(bool& flag, bool& locked_flag, int Key)
{
	//DXP_INPUT_START
	if (GetInputState() & Key)
	{
		if (!locked_flag)
		{
			flag = !flag;
			locked_flag = true;
		}
	} else
		locked_flag = false;

	return flag;
}

int main(void)
{
	if (DxLib_Init() != 0)// 初期化
		exit(0);

	// エラーハンドラの登録
	initExceptionHandler();
	SetDisplayFormat(DXP_FMT_8888); // 色数を16bitに
	ChangeRandMode(DXP_RANDMODE_HW); // 高速化
	ClearDrawScreen(); // 画面消し

	Screen screen(480, 272);

	Bar bar(screen.GetWidth() / 4, 10, DXP_COLOR_GREEN, screen.GetWidth() / 80);
	Blocks block(20);
	BlockBreaker breaker(bar, block);
	Fps fps;

	bool paused = false,
		locked_start = false,
		showed_debug = false,
		locked_select = false;

	while(ProcessMessage() == 0)
	{
		if (!breaker.IsCleared())
		{
			CheckKey(paused, locked_start, DXP_INPUT_START);
			if (paused)
				continue;

			// 画面の初期化
			clsDx();
			ClearDrawScreen();

			// デバッグの出力
			if (CheckKey(showed_debug, locked_select, DXP_INPUT_SELECT))
			{
				printfDx("\nBLOCK=%d\nFALL=%d\nBALL=%d\nFPS=%d\n", //
					breaker.GetCounter().GetBlockCount(), //
					breaker.GetCounter().GetFallCount(), //
					breaker.GetCounter().GetBallCount(), //
					fps.GetFps());
			}

			// 更新
			breaker.Update();

			// 更新
			ScreenFlip();
			fps.Update();
		}else
		{
			//breaker.Redraw();
			//ScreenCopy();

			// 0.5秒
			Sleep(500);
		}
	}
	DxLib_End();
	exit(0);
	return 0;
}

