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
	grade: string | number
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
	const headers: Record<string, string> = {
		'Content-Type': 'application/json',
		...(options.headers as Record<string, string>),
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

const scheduleCache = new Map<string, ScheduleItem[] | ApiError>()
const gradesCache = new Map<string, Grade[] | ApiError>()

async function fetchSchedule(date: string): Promise<ScheduleItem[] | ApiError> {
	if (scheduleCache.has(date)) return scheduleCache.get(date)!
	const result = await apiRequest<ScheduleItem[]>(`/api/schedule?date=${date}`)
	if (!('error' in result)) scheduleCache.set(date, result)
	return result
}

async function fetchGrades(date: string): Promise<Grade[] | ApiError> {
	if (gradesCache.has(date)) return gradesCache.get(date)!
	const result = await apiRequest<Grade[]>(`/api/grades?date=${date}`)
	if (!('error' in result)) gradesCache.set(date, result)
	return result
}

function LoginPage(): string {
	return `
    <div class="login-container">
      <div class="login-card">
        <h1 class="login-title" style="display: flex; align-items: center; justify-content: center; gap: 8px;">
          <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5v-15A2.5 2.5 0 0 1 6.5 2H20v20H6.5a2.5 2.5 0 0 1 0-5H20"></path></svg>
          Журнал Top Academy
        </h1>
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

        <p class="login-note" style="display: flex; align-items: center; justify-content: center; gap: 6px;">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>
          Используйте учетные данные от журнала Компьютерной Академии ТОП
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
          <h1 style="display: flex; align-items: center; gap: 8px;">
            <svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path><circle cx="12" cy="7" r="4"></circle></svg>
            Профиль
          </h1>
          <p class="header-subtitle">${user.login}</p>
        </div>
      </div>
      
      <div class="menu-grid" style="margin-top: 30px;">
        <button class="btn btn-secondary" style="width: 100%; border: 1px solid var(--danger-color); color: var(--danger-color);" id="logout-btn">Выход из аккаунта</button>
        <button class="btn btn-secondary" style="width: 100%; margin-top: 15px;" id="theme-toggle-btn">Сменить тему</button>
      </div>
    </div>
  `
}

function SchedulePage(): string {
	return `
    <div class="page-container">
      <div class="page-header" style="margin-bottom: 20px; display: flex; align-items: center; gap: 8px;">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect><line x1="16" y1="2" x2="16" y2="6"></line><line x1="8" y1="2" x2="8" y2="6"></line><line x1="3" y1="10" x2="21" y2="10"></line></svg>
        <h2 style="font-size: 1.5rem; margin: 0; font-weight: 700;">Расписание</h2>
      </div>

      <div class="schedule-controls" style="display: flex; flex-direction: row; align-items: center; justify-content: space-between; background: var(--bg-color); padding: 15px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px;">
        <button id="prev-day-btn" class="btn btn-secondary" style="padding: 0; border-radius: 50%; border: none; background: var(--bg-secondary); width: 40px; height: 40px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; color: var(--text-primary);">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 18 9 12 15 6"></polyline></svg>
        </button>
        <div style="flex: 1; display: flex; justify-content: center;">
            <input
            type="date"
            id="date-input"
            class="date-input"
            style="border: none; background: transparent; font-size: 16px; font-weight: 600; color: var(--text-primary); text-align: center; width: auto; outline: none; appearance: none;"
            value="${selectedDate}"
            />
        </div>
        <button id="next-day-btn" class="btn btn-secondary" style="padding: 0; border-radius: 50%; border: none; background: var(--bg-secondary); width: 40px; height: 40px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; color: var(--text-primary);">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"></polyline></svg>
        </button>
      </div>
      
      <div id="schedule-content" class="content-area" style="padding-bottom: 80px;">
        <div class="loading">Загрузка расписания...</div>
      </div>
    </div>
  `
}

function GradesPage(): string {
	return `
    <div class="page-container">
      <div class="page-header" style="margin-bottom: 20px; display: flex; align-items: center; gap: 8px;">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="20" x2="18" y2="10"></line><line x1="12" y1="20" x2="12" y2="4"></line><line x1="6" y1="20" x2="6" y2="14"></line></svg>
        <h2 style="font-size: 1.5rem; margin: 0; font-weight: 700;">Оценки</h2>
      </div>
      
      <div class="schedule-controls" style="display: flex; flex-direction: row; align-items: center; justify-content: space-between; background: var(--bg-color); padding: 15px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px;">
        <button id="grades-prev-day-btn" class="btn btn-secondary" style="padding: 0; border-radius: 50%; border: none; background: var(--bg-secondary); width: 40px; height: 40px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; color: var(--text-primary);">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 18 9 12 15 6"></polyline></svg>
        </button>
        <div style="flex: 1; display: flex; justify-content: center;">
            <input
            type="date"
            id="grade-date-input"
            class="date-input"
            style="border: none; background: transparent; font-size: 16px; font-weight: 600; color: var(--text-primary); text-align: center; width: auto; outline: none; appearance: none;"
            value="${selectedDate}"
            />
        </div>
        <button id="grades-next-day-btn" class="btn btn-secondary" style="padding: 0; border-radius: 50%; border: none; background: var(--bg-secondary); width: 40px; height: 40px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; color: var(--text-primary);">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"></polyline></svg>
        </button>
      </div>
      
      <div id="grades-content" class="content-area" style="padding-bottom: 80px;">
        <div class="loading">Загрузка оценок...</div>
      </div>
    </div>
  `
}

function scheduleItemsHtml(items: ScheduleItem[]): string {
	if (items.length === 0) {
		return '<div class="empty-state" style="display: flex; flex-direction: column; align-items: center; gap: 12px; color: var(--text-light);"><svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="22" y1="12" x2="2" y2="12"></line><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"></path><line x1="6" y1="16" x2="6.01" y2="16"></line><line x1="10" y1="16" x2="10.01" y2="16"></line></svg>Нет расписания на эту дату</div>'
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
            <p class="item-room" style="display: flex; align-items: center; gap: 4px;"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"></path><circle cx="12" cy="10" r="3"></circle></svg>Аудитория ${item.room_name}</p>
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
		return '<div class="empty-state" style="display: flex; flex-direction: column; align-items: center; gap: 12px; color: var(--text-light);"><svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="22" y1="12" x2="2" y2="12"></line><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"></path><line x1="6" y1="16" x2="6.01" y2="16"></line><line x1="10" y1="16" x2="10.01" y2="16"></line></svg>Нет оценок за эту дату</div>'
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
	const logoutBtn = document.getElementById('logout-btn')
	const themeToggleBtn = document.getElementById('theme-toggle-btn')

	logoutBtn?.addEventListener('click', logout)
	themeToggleBtn?.addEventListener('click', () => {
		const isDark = localStorage.getItem('theme') !== 'light'
		localStorage.setItem('theme', isDark ? 'light' : 'dark')
		applyTheme()
	})
}

async function setupScheduleHandlers() {
	const dateInput = document.getElementById('date-input') as HTMLInputElement
	const scheduleContent = document.getElementById('schedule-content')
	const prevDayBtn = document.getElementById('prev-day-btn')
	const nextDayBtn = document.getElementById('next-day-btn')

	const changeDay = (offset: number) => {
		const current = new Date(selectedDate)
		current.setDate(current.getDate() + offset)
		selectedDate = current.toISOString().split('T')[0]
		if (dateInput) dateInput.value = selectedDate
		loadSchedule()
	}

	prevDayBtn?.addEventListener('click', () => changeDay(-1))
	nextDayBtn?.addEventListener('click', () => changeDay(1))

	const loadSchedule = async () => {
		selectedDate = dateInput?.value || selectedDate
		if (!selectedDate) selectedDate = new Date().toISOString().split('T')[0]
		if (dateInput) dateInput.value = selectedDate

		if (!scheduleCache.has(selectedDate)) {
			scheduleContent!.innerHTML = '<div class="loading">Загрузка...</div>'
		}

		const result = await fetchSchedule(selectedDate)
		if ('error' in result) {
			scheduleContent!.innerHTML = `<div class="error-state" style="display: flex; flex-direction: column; align-items: center; gap: 12px; color: var(--danger-color);"><svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line></svg>Ошибка: ${result.error}</div>`
		} else {
			scheduleContent!.innerHTML = scheduleItemsHtml(result)
		}
	}

	dateInput?.addEventListener('change', loadSchedule)
	await loadSchedule()
}

async function setupGradesHandlers() {
	const dateInput = document.getElementById('grade-date-input') as HTMLInputElement
	const gradesContent = document.getElementById('grades-content')
	const prevDayBtn = document.getElementById('grades-prev-day-btn')
	const nextDayBtn = document.getElementById('grades-next-day-btn')

	const changeDay = (offset: number) => {
		const current = new Date(selectedDate)
		current.setDate(current.getDate() + offset)
		selectedDate = current.toISOString().split('T')[0]
		if (dateInput) dateInput.value = selectedDate
		loadGrades()
	}

	prevDayBtn?.addEventListener('click', () => changeDay(-1))
	nextDayBtn?.addEventListener('click', () => changeDay(1))

	const loadGrades = async () => {
		selectedDate = dateInput?.value || selectedDate
		if (!selectedDate) selectedDate = new Date().toISOString().split('T')[0]
		if (dateInput) dateInput.value = selectedDate

		if (!gradesCache.has(selectedDate)) {
			gradesContent!.innerHTML = '<div class="loading">Загрузка...</div>'
		}

		const result = await fetchGrades(selectedDate)
		if ('error' in result) {
			gradesContent!.innerHTML = `<div class="error-state" style="display: flex; flex-direction: column; align-items: center; gap: 12px; color: var(--danger-color);"><svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line></svg>Ошибка: ${result.error}</div>`
		} else {
			gradesContent!.innerHTML = gradesHtml(result)
		}
	}

	dateInput?.addEventListener('change', loadGrades)

	await loadGrades()
}

function renderNav() {
	if (currentPage === 'login') return ''
	
	const isActive = (page: string) => currentPage === page ? 'color: var(--primary-color);' : 'color: var(--text-secondary);'
		const navHtml = `
		<nav style="position: fixed; bottom: 0; left: 0; right: 0; background-color: var(--bg-color); backdrop-filter: blur(10px); display: flex; justify-content: space-around; padding: 12px 0 25px 0; box-shadow: 0 -1px 0 rgba(0,0,0,0.1); border-top: 1px solid var(--border-color); z-index: 1000;">
			<div onclick="window.navigatePage('schedule')" style="text-align: center; cursor: pointer; transition: 0.2s; display: flex; flex-direction: column; align-items: center; ${isActive('schedule')}">
				<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-bottom: 4px;"><rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect><line x1="16" y1="2" x2="16" y2="6"></line><line x1="8" y1="2" x2="8" y2="6"></line><line x1="3" y1="10" x2="21" y2="10"></line></svg>
				<div style="font-size: 11px; font-weight: 600;">Уроки</div>
			</div>
			<div onclick="window.navigatePage('grades')" style="text-align: center; cursor: pointer; transition: 0.2s; display: flex; flex-direction: column; align-items: center; ${isActive('grades')}">
				<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-bottom: 4px;"><line x1="18" y1="20" x2="18" y2="10"></line><line x1="12" y1="20" x2="12" y2="4"></line><line x1="6" y1="20" x2="6" y2="14"></line></svg>
				<div style="font-size: 11px; font-weight: 600;">Оценки</div>
			</div>
			<div onclick="window.navigatePage('dashboard')" style="text-align: center; cursor: pointer; transition: 0.2s; display: flex; flex-direction: column; align-items: center; ${isActive('dashboard')}">
				<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="margin-bottom: 4px;"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path><circle cx="12" cy="7" r="4"></circle></svg>
				<div style="font-size: 11px; font-weight: 600;">Профиль</div>
			</div>
		</nav>
	`
	return navHtml
}

;(window as any).navigatePage = (page: AppPage) => {
	currentPage = page
	render()
}

function applyTheme() {
	const isDark = localStorage.getItem('theme') !== 'light' // default dark
	if (isDark) {
		document.body.classList.add('dark-theme')
		document.body.classList.remove('light-theme')
	} else {
		document.body.classList.remove('dark-theme')
		document.body.classList.add('light-theme')
	}
}

function render() {
	const root = document.getElementById('root')!

	applyTheme()

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

	root.innerHTML = html + renderNav()

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
