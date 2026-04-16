# Build stage
FROM node:20-alpine as builder

WORKDIR /build

COPY package.json package-lock.json* ./

RUN npm ci --only=production

COPY . .

RUN npm run build

# Production stage
FROM node:20-alpine

WORKDIR /app

# Install serve or http-server for production
RUN npm install -g serve

# Copy built files
COPY --from=builder /build/dist ./dist
COPY --from=builder /build/node_modules ./node_modules

EXPOSE 3000

ENV HOST=0.0.0.0
ENV PORT=3000

# Start server
CMD ["serve", "-s", "dist", "-l", "3000", "--single"]
