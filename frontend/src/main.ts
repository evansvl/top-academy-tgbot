declare global {
	interface Window {
		Telegram: any
	}
}

import './style.css'

window.Telegram.WebApp.ready()
window.Telegram.WebApp.expand()

async function init() {
	const app = document.querySelector<HTMLDivElement>('#app')!
	const tg = window.Telegram.WebApp
	const initData = tg.initData || ''

	app.innerHTML = `
    <div class="header">
      <h2>Расписание</h2>
    </div>
    <div id="content">Загрузка...</div>
  `

	try {
		const res = await fetch('/api/tma/schedule', {
			headers: {
				'x-telegram-initdata': initData,
			},
		})

		if (!res.ok) {
			app.querySelector('#content')!.innerHTML =
				'<p class="error">Ошибка авторизации или загрузки</p>'
			return
		}

		const data = await res.json()

		let html = ''

		if (data.error) {
			html = `<p class="error">${data.error}</p>`
		} else {
			html = '<ul class="schedule-list">'
			for (const item of data) {
				html += `
           <li class="schedule-item">
             <div class="time">${item.started_at} - ${item.finished_at}</div>
             <div class="subject">${item.subject_name}</div>
             <div class="room">${item.room_name}</div>
           </li>
         `
			}
			html += '</ul>'
		}

		app.querySelector('#content')!.innerHTML = html
	} catch (e) {
		app.querySelector('#content')!.innerHTML =
			'<p class="error">Произошла ошибка подключения</p>'
	}
}

init()
