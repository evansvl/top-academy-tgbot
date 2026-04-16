import './style.css'

interface User {
	telegram_id: number
	login: string
	password: string
	access_token: string
	refresh_token: string
	student_id: number
	group_id: number
}

interface ScheduleItem {
	started_at: string
	finished_at: string
	subject_name: string
	room_name: string
}

interface Grade {
	subject: string
	grade: number
	date: string
}

interface ApiError {
	error: string
}

type AppPage = 'login' | 'dashboard' | 'schedule' | 'grades'

let currentUser: User | null = null
let currentPage: AppPage = 'login'
let selectedDate = new Date().toISOString().split('T')[0]

function loadUser() {
	const stored = localStorage.getItem('user')
	if (stored) {
		try {
			currentUser = JSON.parse(stored)
		} catch (e) {
			currentUser = null
		}
	}
}

function saveUser(user: User | null) {
	if (user) {
		localStorage.setItem('user', JSON.stringify(user))
	} else {
		localStorage.removeItem('user')
	}
	currentUser = user
}

async function apiRequest<T>(
	endpoint: string,
	options: RequestInit & { initData?: string } = {}
): Promise<T | ApiError> {
	const headers: HeadersInit = {
		'Content-Type': 'application/json',
		...options.headers,
	}

	if (options.initData) {
		headers['x-telegram-initdata'] = options.initData
	} else if (currentUser) {
		headers['Authorization'] = `Bearer ${currentUser.access_token}`
	}

	try {
		const response = await fetch(endpoint, {
			...options,
			headers,
		})

		if (!response.ok) {
			return { error: `HTTP ${response.status}` }
		}

		return (await response.json()) as T
	} catch (error) {
		return { error: String(error) }
	}
}

async function login(username: string, password: string): Promise<boolean> {
	try {
		const response = await fetch('/api/auth/login', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ login: username, password }),
		})

		if (!response.ok) {
			return false
		}

		const user = (await response.json()) as User
		saveUser(user)
		return true
	} catch (error) {
		console.error('Login error:', error)
		return false
	}
}

async function logout() {
	saveUser(null)
	currentPage = 'login'
	render()
}

async function fetchSchedule(date: string): Promise<ScheduleItem[] | ApiError> {
	return apiRequest<ScheduleItem[]>(`/api/schedule?date=${date}`)
}

async function fetchGrades(month: number, year: number): Promise<Grade[] | ApiError> {
	return apiRequest<Grade[]>(`/api/grades?month=${month}&year=${year}`)
}

function LoginPage(): string {
	return `
    <div class="login-container">
      <div class="login-card">
        <h1 class="login-title">📚 Журнал Top Academy</h1>
        <p class="login-subtitle">Вход в личный кабинет</p>
        
        <form id="login-form" class="login-form">
          <div class="form-group">
            <label for="username" class="form-label">Логин</label>
            <input
              type="text"
              id="username"
              placeholder="Введите ваш логин"
              class="form-input"
              required
              autofocus
            />
          </div>
          
          <div class="form-group">
            <label for="password" class="form-label">Пароль</label>
            <input
              type="password"
              id="password"
              placeholder="Введите ваш пароль"
              class="form-input"
              required
            />
          </div>
          
          <div id="login-error" class="form-error" style="display: none;"></div>
          
          <button type="submit" class="btn btn-primary" id="login-btn">
            Войти
          </button>
        </form>
        
        <p class="login-note">
          💡 Используйте учетные данные от журнала Компьютерной Академии ТОП
        </p>
      </div>
    </div>
  `
}

function DashboardPage(): string {
	const user = currentUser!
	return `
    <div class="dashboard">
      <div class="dashboard-header">
        <div class="header-content">
          <h1>👋 Добро пожаловать!</h1>
          <p class="header-subtitle">${user.login}</p>
        </div>
        <button class="btn btn-secondary btn-small" id="logout-btn">Выход</button>
      </div>
      
      <div class="menu-grid">
        <div class="menu-card" id="schedule-card">
          <div class="menu-icon">📅</div>
          <h3>Расписание</h3>
          <p>Просмотрите расписание занятий</p>
        </div>
        
        <div class="menu-card" id="grades-card">
          <div class="menu-icon">📊</div>
          <h3>Оценки</h3>
          <p>Просмотрите ваши оценки</p>
        </div>
      </div>
    </div>
  `
}

function SchedulePage(): string {
	return `
    <div class="page-container">
      <div class="page-header">
        <button class="btn btn-back" id="back-btn">← Назад</button>
        <h1>📅 Расписание</h1>
      </div>
      
      <div class="schedule-controls">
        <label for="date-input" class="control-label">Выберите дату:</label>
        <input
          type="date"
          id="date-input"
          class="date-input"
          value="${selectedDate}"
        />
      </div>
      
      <div id="schedule-content" class="content-area">
        <div class="loading">Загрузка расписания...</div>
      </div>
    </div>
  `
}

function GradesPage(): string {
	const now = new Date()
	return `
    <div class="page-container">
      <div class="page-header">
        <button class="btn btn-back" id="back-btn">← Назад</button>
        <h1>📊 Оценки</h1>
      </div>
      
      <div class="grades-controls">
        <div class="control-group">
          <label for="month-select" class="control-label">Месяц:</label>
          <select id="month-select" class="form-input">
            <option value="1">Январь</option>
            <option value="2">Февраль</option>
            <option value="3">Март</option>
            <option value="4" selected>Апрель</option>
            <option value="5">Май</option>
            <option value="6">Июнь</option>
            <option value="7">Июль</option>
            <option value="8">Август</option>
            <option value="9">Сентябрь</option>
            <option value="10">Октябрь</option>
            <option value="11">Ноябрь</option>
            <option value="12">Декабрь</option>
          </select>
        </div>
        
        <div class="control-group">
          <label for="year-select" class="control-label">Год:</label>
          <select id="year-select" class="form-input">
            ${[now.getFullYear() - 1, now.getFullYear(), now.getFullYear() + 1]
			.map((y) => `<option value="${y}" ${y === now.getFullYear() ? 'selected' : ''}>${y}</option>`)
			.join('')}
          </select>
        </div>
      </div>
      
      <div id="grades-content" class="content-area">
        <div class="loading">Загрузка оценок...</div>
      </div>
    </div>
  `
}

function scheduleItemsHtml(items: ScheduleItem[]): string {
	if (items.length === 0) {
		return '<div class="empty-state">📭 Нет расписания на эту дату</div>'
	}

	return `
    <div class="schedule-list">
      ${items
		.map(
			(item) => `
        <div class="schedule-item">
          <div class="item-time">
            <span class="time-start">${item.started_at}</span>
            <span class="time-separator">—</span>
            <span class="time-end">${item.finished_at}</span>
          </div>
          <div class="item-content">
            <h4 class="item-subject">${item.subject_name}</h4>
            <p class="item-room">📍 Аудитория ${item.room_name}</p>
          </div>
        </div>
      `
		)
		.join('')}
    </div>
  `
}

function gradesHtml(grades: Grade[]): string {
	if (grades.length === 0) {
		return '<div class="empty-state">📭 Нет оценок за этот период</div>'
	}

	const grouped: { [key: string]: Grade[] } = {}
	grades.forEach((g) => {
		if (!grouped[g.subject]) {
			grouped[g.subject] = []
		}
		grouped[g.subject].push(g)
	})

	return `
    <div class="grades-list">
      ${Object.entries(grouped)
		.map(
			([subject, subjectGrades]) => `
        <div class="grades-subject">
          <h4 class="subject-name">${subject}</h4>
          <div class="grades-items">
            ${subjectGrades.map((g) => `<span class="grade-badge grade-${g.grade}">${g.grade}</span>`).join('')}
          </div>
        </div>
      `
		)
		.join('')}
    </div>
  `
}

function setupLoginHandlers() {
	const form = document.getElementById('login-form') as HTMLFormElement
	const errorDiv = document.getElementById('login-error') as HTMLDivElement
	const loginBtn = document.getElementById('login-btn') as HTMLButtonElement

	form?.addEventListener('submit', async (e) => {
		e.preventDefault()
		const username = (document.getElementById('username') as HTMLInputElement)?.value
		const password = (document.getElementById('password') as HTMLInputElement)?.value

		if (!username || !password) return

		loginBtn.disabled = true
		loginBtn.textContent = 'Проверка...'
		errorDiv.style.display = 'none'

		const success = await login(username, password)

		if (success) {
			currentPage = 'dashboard'
			render()
		} else {
			errorDiv.textContent = '❌ Неверный логин или пароль'
			errorDiv.style.display = 'block'
			loginBtn.disabled = false
			loginBtn.textContent = 'Войти'
		}
	})
}

function setupDashboardHandlers() {
	const scheduleCard = document.getElementById('schedule-card')
	const gradesCard = document.getElementById('grades-card')
	const logoutBtn = document.getElementById('logout-btn')

	scheduleCard?.addEventListener('click', () => {
		currentPage = 'schedule'
		render()
	})

	gradesCard?.addEventListener('click', () => {
		currentPage = 'grades'
		render()
	})

	logoutBtn?.addEventListener('click', logout)
}

async function setupScheduleHandlers() {
	const backBtn = document.getElementById('back-btn')
	const dateInput = document.getElementById('date-input') as HTMLInputElement
	const scheduleContent = document.getElementById('schedule-content')

	backBtn?.addEventListener('click', () => {
		currentPage = 'dashboard'
		render()
	})

	const loadSchedule = async () => {
		if (dateInput?.value) {
			selectedDate = dateInput.value
			scheduleContent!.innerHTML = '<div class="loading">Загрузка...</div>'

			const result = await fetchSchedule(selectedDate)
			if ('error' in result) {
				scheduleContent!.innerHTML = `<div class="error-state">⚠️ Ошибка: ${result.error}</div>`
			} else {
				scheduleContent!.innerHTML = scheduleItemsHtml(result)
			}
		}
	}

	dateInput?.addEventListener('change', loadSchedule)
	await loadSchedule()
}

async function setupGradesHandlers() {
	const backBtn = document.getElementById('back-btn')
	const monthSelect = document.getElementById('month-select') as HTMLSelectElement
	const yearSelect = document.getElementById('year-select') as HTMLSelectElement
	const gradesContent = document.getElementById('grades-content')

	backBtn?.addEventListener('click', () => {
		currentPage = 'dashboard'
		render()
	})

	const loadGrades = async () => {
		const month = parseInt(monthSelect?.value || '1')
		const year = parseInt(yearSelect?.value || new Date().getFullYear().toString())

		gradesContent!.innerHTML = '<div class="loading">Загрузка...</div>'

		const result = await fetchGrades(month, year)
		if ('error' in result) {
			gradesContent!.innerHTML = `<div class="error-state">⚠️ Ошибка: ${result.error}</div>`
		} else {
			gradesContent!.innerHTML = gradesHtml(result)
		}
	}

	monthSelect?.addEventListener('change', loadGrades)
	yearSelect?.addEventListener('change', loadGrades)
	await loadGrades()
}

function render() {
	const root = document.getElementById('root')!

	if (!currentUser) {
		currentPage = 'login'
	}

	let html = ''
	switch (currentPage) {
		case 'login':
			html = LoginPage()
			break
		case 'dashboard':
			html = DashboardPage()
			break
		case 'schedule':
			html = SchedulePage()
			break
		case 'grades':
			html = GradesPage()
			break
	}

	root.innerHTML = html

	// Setup event handlers based on page
	setTimeout(() => {
		switch (currentPage) {
			case 'login':
				setupLoginHandlers()
				break
			case 'dashboard':
				setupDashboardHandlers()
				break
			case 'schedule':
				setupScheduleHandlers()
				break
			case 'grades':
				setupGradesHandlers()
				break
		}
	}, 0)
}

// ============================================================================
// INITIALIZATION
// ============================================================================

loadUser()

if (currentUser) {
	currentPage = 'dashboard'
} else {
	currentPage = 'login'
}

render()
