import { defineConfig } from 'vite'

export default defineConfig({
	server: {
		port: 3000,
		host: '0.0.0.0',
		proxy: {
			'/api': {
				target: process.env.VITE_API_URL || 'http://localhost:8080',
				changeOrigin: true,
				rewrite: (path) => path,
			},
		},
	},
})
