FROM node:20-alpine as builder

WORKDIR /build

COPY package.json ./

RUN npm install

COPY . .

RUN npm run build

FROM node:20-alpine

WORKDIR /app

RUN npm install -g serve

COPY --from=builder /build/dist ./dist
COPY --from=builder /build/node_modules ./node_modules

EXPOSE 3000

ENV HOST=0.0.0.0
ENV PORT=3000

CMD ["serve", "-s", "dist", "-l", "3000", "--single"]
